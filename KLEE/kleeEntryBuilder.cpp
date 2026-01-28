/**
 * ============================================================================
 * KLEE Entry Builder
 * ============================================================================
 * 
 * Overview:
 * This is an entry builder for the KLEE symbolic execution engine. Its primary
 * function is to generate a symbolic execution entry function (klee_entry) for
 * target programs, which initializes global variables and heap memory, establishes
 * pointer relationships, and invokes the function to be verified.
 * 
 * Core Functional Modules:
 * 
 * 1. API Function Declaration Creation (createAPIFunctionDeclares)
 *    - Creates necessary external function declarations: memcpy, klee_make_symbolic, printf
 * 
 * 2. Heap Memory Management (createHeapBytes)
 *    - Creates global byte array heap_bytes to simulate heap memory based on model inputs
 *    - Calculates total size of all heap variables and allocates space
 * 
 * 3. Helper Function Generation (createAppendToBytesFunctions)
 *    - Creates append_*_to_bytes series functions for appending values to byte arrays
 *    - Supports i8, i16, i32, i64, float, double types
 * 
 * 4. Global Variable Collection (collectAllGlobals)
 *    - Collects global variables marked with global_vars.XX section in LLVM IR
 *    - Calculates total size and verifies count
 * 
 * 5. Pointer Value Processing (processPtrValues)
 *    - Analyzes all pointer values in model inputs
 *    - Establishes address-to-memory-region mappings (e.g., "copter + 32152")
 *    - Identifies pointer targets (global variables, heap, or constant globals)
 * 
 * 6. Pointer Relationship Handling (handlePointToAddr)
 *    - Parses strings in "variable_name + offset" format
 *    - Returns pairs of target global variable and offset
 * 
 * 7. Pointer Concretization (concretizePointer)
 *    - Inserts code in entry function to set pointers to correct memory locations
 *    - Establishes linkage relationships for pointer members
 * 
 * 8. Symbolization Processing
 *    - symbolicBasicTyGVs: Marks basic-type global variables as symbolic
 *    - symbolicBasicTyHeapVars: Marks basic-type heap variables as symbolic
 *    - symbolicAddrRange: Symbolizes specified memory address ranges
 *      (Special handling for arducopter module, only symbolizing "copter" variable)
 * 
 * 9. Global Variable and Heap Concretization (concretizeGlobalsAndHeap)
 *    - Initializes global variables and heap memory with concrete values from model inputs
 *    - Handles members of aggregate types (structs)
 *    - Supports integer, float, double types
 * 
 * 10. Global Variable Analysis (analysisGVs)
 *     - Analyzes structure of all global and heap variables
 *     - Identifies pointer types and aggregate types
 *     - Establishes pointer relationship list and computes address ranges for symbolization
 *     - Processes heap memory as a whole, merging overlapping pointer ranges
 * 
 * 11. ELF File Parsing (parseELFFile)
 *     - Uses LLVM Object library to parse ELF executable files
 *     - Extracts symbol table information (addresses and sizes)
 *     - Matches constant global variables in IR with ELF symbols
 *     - Special handling for string constants, finds longest string constant
 *     - Stores address range information as LLVM metadata
 * 
 * 12. KLEE Entry Function Creation (createKleeEntry)
 *     Core function executing in order:
 *     a. Analyze structure of global and heap variables
 *     b. Concretize initial values of global variables and heap
 *     c. Establish pointer relationships (pointer concretization)
 *     d. Symbolize memory regions to be explored
 *     e. Call target entry function (function to be verified)
 * 
 * Workflow:
 * 1. Parse protobuf model inputs → obtain global variables, heap variables, parameters
 * 2. Parse ELF file → obtain address ranges of constant global variables
 * 3. Collect global variables in LLVM IR
 * 4. Analyze variable structure and pointer relationships
 * 5. Generate klee_entry function: concretize known values → establish pointer linkages
 *    → symbolize non-pointer regions → call target function
 * 
 * ============================================================================
 */

#include "kleeEntryBuilder.hh"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Error.h"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include "config_parse.hpp"
#include "user_utils.hh"

namespace KLEE{

    // extern std::vector<xml::Variable> xml::globalVars;
    // extern std::vector<xml::Variable> xml::heapVars;

    void kleeEntryBuilder::createAPIFunctionDeclares(llvm::Module &M){
        // create 'declare void @memcpy(ptr, ptr, i64)'
        _memcpy = M.getFunction("memcpy");
        if (_memcpy == nullptr){
            _memcpy = llvm::Function::Create(
                llvm::FunctionType::get(
                    llvm::Type::getVoidTy(M.getContext()),
                    {llvm::Type::getInt8PtrTy(M.getContext()), llvm::Type::getInt8PtrTy(M.getContext()), llvm::Type::getInt64Ty(M.getContext())},
                    false
                ),
                llvm::GlobalValue::ExternalLinkage,
                "memcpy",
                &M
            );
        }

        // create 'declare void @klee_make_symbolic(ptr noundef, i64 noundef, ptr noundef)'
        llvm::Type* ptrType = llvm::Type::getInt8PtrTy(M.getContext());
        llvm::Type* i64Type = llvm::Type::getInt64Ty(M.getContext());
        std::vector<llvm::Type*> args = {ptrType, i64Type, ptrType};
        llvm::FunctionType* kleeMakeSymbolicType = llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), args, false);
        _kleeMakeSymbolic = llvm::Function::Create(kleeMakeSymbolicType, llvm::GlobalValue::ExternalLinkage, "klee_make_symbolic", &M);

        // check printf is declared
        if (M.getFunction("printf") == nullptr){
            // create declare i32 @printf(ptr, ...)
            llvm::FunctionType* printfType = llvm::FunctionType::get(llvm::Type::getInt32Ty(M.getContext()), {llvm::Type::getInt8PtrTy(M.getContext())}, true);
            M.getFunctionList().push_back(llvm::Function::Create(printfType, llvm::GlobalValue::ExternalLinkage, "printf", &M));
        }
    }

    void kleeEntryBuilder::createHeapBytes(llvm::Module &M){
        // create heap bytes
        for (auto &heapVar : _modelInputs.heapvars()){
            _hsize += heapVar.size();
        }

        std::string heapBytesName = "heap_bytes";
        llvm::Type* hbytesType = llvm::ArrayType::get(llvm::Type::getInt8Ty(M.getContext()), _hsize);
        llvm::GlobalVariable* hbytes = new llvm::GlobalVariable(M, hbytesType, false, llvm::GlobalValue::ExternalLinkage, nullptr, "heap");
        llvm::Constant* zero = llvm::Constant::getNullValue(hbytesType);
        hbytes->setInitializer(zero);
        _heapBytes = hbytes;
    }
   
    void kleeEntryBuilder::createAppendToBytesFunctions(llvm::Module &M) {
        // create append_i8_to_bytes function
        llvm::FunctionType* append_i8_to_bytesType = llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), {llvm::Type::getInt8PtrTy(M.getContext()), llvm::Type::getInt64Ty(M.getContext()), llvm::Type::getInt8Ty(M.getContext())}, false);
        _append_i8_to_bytes = llvm::Function::Create(append_i8_to_bytesType, llvm::GlobalValue::InternalLinkage, "append_i8_to_bytes", M);
        llvm::BasicBlock* entry_i8 = llvm::BasicBlock::Create(M.getContext(), "entry", _append_i8_to_bytes);
        llvm::IRBuilder<> builder_i8(M.getContext());
        builder_i8.SetInsertPoint(entry_i8);
        auto argIt_i8 = _append_i8_to_bytes->arg_begin();
        llvm::Value* ptr_i8 = &*argIt_i8++;
        llvm::Value* offset_i8 = &*argIt_i8++;
        llvm::Value* value_i8 = &*argIt_i8;
        llvm::Value* gep_i8 = builder_i8.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), ptr_i8, offset_i8);
        builder_i8.CreateStore(value_i8, gep_i8);
        builder_i8.CreateRetVoid();
        _append_i8_to_bytes->setDSOLocal(true);
    
        // create append_i16_to_bytes function
        llvm::FunctionType* append_i16_to_bytesType = llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), {llvm::Type::getInt8PtrTy(M.getContext()), llvm::Type::getInt64Ty(M.getContext()), llvm::Type::getInt16Ty(M.getContext())}, false);
        _append_i16_to_bytes = llvm::Function::Create(append_i16_to_bytesType, llvm::GlobalValue::InternalLinkage, "append_i16_to_bytes", M);
        llvm::BasicBlock* entry_i16 = llvm::BasicBlock::Create(M.getContext(), "entry", _append_i16_to_bytes);
        llvm::IRBuilder<> builder_i16(M.getContext());
        builder_i16.SetInsertPoint(entry_i16);
        auto argIt_i16 = _append_i16_to_bytes->arg_begin();
        llvm::Value* ptr_i16 = &*argIt_i16++;
        llvm::Value* offset_i16 = &*argIt_i16++;
        llvm::Value* value_i16 = &*argIt_i16;
        llvm::Value* gep_i16 = builder_i16.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), ptr_i16, offset_i16);
        builder_i16.CreateStore(value_i16, gep_i16);
        builder_i16.CreateRetVoid();
        _append_i16_to_bytes->setDSOLocal(true);
    
        // create append_i32_to_bytes function
        llvm::FunctionType* append_i32_to_bytesType = llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), {llvm::Type::getInt8PtrTy(M.getContext()), llvm::Type::getInt64Ty(M.getContext()), llvm::Type::getInt32Ty(M.getContext())}, false);
        _append_i32_to_bytes = llvm::Function::Create(append_i32_to_bytesType, llvm::GlobalValue::InternalLinkage, "append_i32_to_bytes", M);
        llvm::BasicBlock* entry_i32 = llvm::BasicBlock::Create(M.getContext(), "entry", _append_i32_to_bytes);
        llvm::IRBuilder<> builder_i32(M.getContext());
        builder_i32.SetInsertPoint(entry_i32);
        auto argIt_i32 = _append_i32_to_bytes->arg_begin();
        llvm::Value* ptr_i32 = &*argIt_i32++;
        llvm::Value* offset_i32 = &*argIt_i32++;
        llvm::Value* value_i32 = &*argIt_i32;
        llvm::Value* gep_i32 = builder_i32.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), ptr_i32, offset_i32);
        builder_i32.CreateStore(value_i32, gep_i32);
        builder_i32.CreateRetVoid();
        _append_i32_to_bytes->setDSOLocal(true);
    
        // create append_i64_to_bytes function
        llvm::FunctionType* append_i64_to_bytesType = llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), {llvm::Type::getInt8PtrTy(M.getContext()), llvm::Type::getInt64Ty(M.getContext()), llvm::Type::getInt64Ty(M.getContext())}, false);
        _append_i64_to_bytes = llvm::Function::Create(append_i64_to_bytesType, llvm::GlobalValue::InternalLinkage, "append_i64_to_bytes", M);
        llvm::BasicBlock* entry_i64 = llvm::BasicBlock::Create(M.getContext(), "entry", _append_i64_to_bytes);
        llvm::IRBuilder<> builder_i64(M.getContext());
        builder_i64.SetInsertPoint(entry_i64);
        auto argIt_i64 = _append_i64_to_bytes->arg_begin();
        llvm::Value* ptr_i64 = &*argIt_i64++;
        llvm::Value* offset_i64 = &*argIt_i64++;
        llvm::Value* value_i64 = &*argIt_i64;
        llvm::Value* gep_i64 = builder_i64.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), ptr_i64, offset_i64);
        builder_i64.CreateStore(value_i64, gep_i64);
        builder_i64.CreateRetVoid();
        _append_i64_to_bytes->setDSOLocal(true);
    
        // create append_float_to_bytes function
        llvm::FunctionType* append_float_to_bytesType = llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), {llvm::Type::getInt8PtrTy(M.getContext()), llvm::Type::getInt64Ty(M.getContext()), llvm::Type::getFloatTy(M.getContext())}, false);
        _append_float_to_bytes = llvm::Function::Create(append_float_to_bytesType, llvm::GlobalValue::InternalLinkage, "append_float_to_bytes", M);
        llvm::BasicBlock* entry_float = llvm::BasicBlock::Create(M.getContext(), "entry", _append_float_to_bytes);
        llvm::IRBuilder<> builder_float(M.getContext());
        builder_float.SetInsertPoint(entry_float);
        auto argIt_float = _append_float_to_bytes->arg_begin();
        llvm::Value* ptr_float = &*argIt_float++;
        llvm::Value* offset_float = &*argIt_float++;
        llvm::Value* value_float = &*argIt_float;
        llvm::Value* gep_float = builder_float.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), ptr_float, offset_float);
        builder_float.CreateStore(value_float, gep_float);
        builder_float.CreateRetVoid();
        _append_float_to_bytes->setDSOLocal(true);
    
        // create append_double_to_bytes function
        llvm::FunctionType* append_double_to_bytesType = llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), {llvm::Type::getInt8PtrTy(M.getContext()), llvm::Type::getInt64Ty(M.getContext()), llvm::Type::getDoubleTy(M.getContext())}, false);
        _append_double_to_bytes = llvm::Function::Create(append_double_to_bytesType, llvm::GlobalValue::InternalLinkage, "append_double_to_bytes", M);
        llvm::BasicBlock* entry_double = llvm::BasicBlock::Create(M.getContext(), "entry", _append_double_to_bytes);
        llvm::IRBuilder<> builder_double(M.getContext());
        builder_double.SetInsertPoint(entry_double);
        auto argIt_double = _append_double_to_bytes->arg_begin();
        llvm::Value* ptr_double = &*argIt_double++;
        llvm::Value* offset_double = &*argIt_double++;
        llvm::Value* value_double = &*argIt_double;
        llvm::Value* gep_double = builder_double.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), ptr_double, offset_double);
        builder_double.CreateStore(value_double, gep_double);
        builder_double.CreateRetVoid();
        _append_double_to_bytes->setDSOLocal(true);
    }

    void kleeEntryBuilder::collectAllGlobals(llvm::Module &M){
        for (auto &GV : M.globals()){
            // Only collect global variables within 'global_vars.XX' section.
            if (!GV.getSection().contains("global_vars.")) {
                continue;
            }
            _allGlobals[GV.getName().str()] = &GV;
            _gvsize += M.getDataLayout().getTypeAllocSize(GV.getValueType());
        }

        assert(_allGlobals.size() == _modelInputs.globalvars_size() && "global numbers not match");
    }

    void kleeEntryBuilder::loadParamsConfig(llvm::Module &M) {
        std::string dtmc = utils::getEnv("DTMC");
        if (dtmc.empty()) {
            ERROR("DTMC environment variable not set");
            abort();
        }
        
        std::string configPath = dtmc + "/InterpreterR/base/params_config.yml";
        
        if (!std::filesystem::exists(configPath)) {
            INFO("No params_config.yml found at: " << configPath);
            return;
        }
        
        try {
            YAML::Node config = YAML::LoadFile(configPath);
            
            // Get property name from IR config
            std::string IR_config_path = dtmc + "/configs/IR_config.yml";
            IRConfig irConfig = parseConfig(IR_config_path);
            std::string property_name = irConfig.base.property_name;
            
            if (!config[property_name]) {
                INFO("No configuration for property '" << property_name << "' in params_config.yml");
                return;
            }
            
            YAML::Node propertyConfig = config[property_name];
            if (!propertyConfig["params"]) {
                ERROR("No 'params' field found for property '" << property_name << "' in params_config.yml");
                abort();
            }
            
            YAML::Node params = propertyConfig["params"];
            llvm::LLVMContext& Context = M.getContext();
            
            for (const auto& param : params) {
                int id = param["id"].as<int>();
                std::string typeStr = param["type"].as<std::string>();
                std::string paramGVName = "params_config_" + std::to_string(id);
                
                llvm::GlobalVariable* paramGV = nullptr;
                size_t dataSize = 0;
                
                // Check if it's an aggregate type (struct, class)
                if (typeStr.find("struct.") == 0 || typeStr.find("class.") == 0) {
                    // Aggregate type - store as byte array
                    if (!param["value"].IsSequence()) {
                        ERROR("Aggregate type '" << typeStr << "' requires array value");
                        abort();
                    }
                    
                    std::vector<uint8_t> bytes = param["value"].as<std::vector<uint8_t>>();
                    dataSize = bytes.size();
                    
                    // Create global byte array
                    llvm::ArrayType* arrayType = llvm::ArrayType::get(llvm::Type::getInt8Ty(Context), dataSize);
                    
                    // Create initializer with the data
                    std::vector<llvm::Constant*> initValues;
                    for (uint8_t byte : bytes) {
                        initValues.push_back(llvm::ConstantInt::get(llvm::Type::getInt8Ty(Context), byte));
                    }
                    llvm::Constant* initializer = llvm::ConstantArray::get(arrayType, initValues);
                    
                    paramGV = new llvm::GlobalVariable(
                        M,
                        arrayType,
                        true,  // constant - these are configuration parameters that should not be modified
                        llvm::GlobalValue::InternalLinkage,
                        initializer,
                        paramGVName
                    );
                    
                    INFO("Created params_config global variable: " << paramGVName << " (aggregate, " << dataSize << " bytes)");
                } else {
                    // Scalar type
                    llvm::Type* llvmType = nullptr;
                    llvm::Constant* initValue = nullptr;
                    
                    if (typeStr == "i1") {
                        llvmType = llvm::Type::getInt1Ty(Context);
                        initValue = llvm::ConstantInt::get(llvmType, param["value"].as<uint64_t>());
                        dataSize = 1;
                    } else if (typeStr == "i8") {
                        llvmType = llvm::Type::getInt8Ty(Context);
                        initValue = llvm::ConstantInt::get(llvmType, param["value"].as<uint64_t>());
                        dataSize = 1;
                    } else if (typeStr == "i16") {
                        llvmType = llvm::Type::getInt16Ty(Context);
                        initValue = llvm::ConstantInt::get(llvmType, param["value"].as<uint64_t>());
                        dataSize = 2;
                    } else if (typeStr == "i32") {
                        llvmType = llvm::Type::getInt32Ty(Context);
                        initValue = llvm::ConstantInt::get(llvmType, param["value"].as<uint64_t>());
                        dataSize = 4;
                    } else if (typeStr == "i64") {
                        llvmType = llvm::Type::getInt64Ty(Context);
                        initValue = llvm::ConstantInt::get(llvmType, param["value"].as<uint64_t>());
                        dataSize = 8;
                    } else if (typeStr == "float") {
                        llvmType = llvm::Type::getFloatTy(Context);
                        initValue = llvm::ConstantFP::get(llvmType, param["value"].as<float>());
                        dataSize = 4;
                    } else if (typeStr == "double") {
                        llvmType = llvm::Type::getDoubleTy(Context);
                        initValue = llvm::ConstantFP::get(llvmType, param["value"].as<double>());
                        dataSize = 8;
                    } else {
                        ERROR("Unsupported scalar type: " << typeStr);
                        abort();
                    }
                    
                    paramGV = new llvm::GlobalVariable(
                        M,
                        llvmType,
                        true,  // constant - these are configuration parameters that should not be modified
                        llvm::GlobalValue::InternalLinkage,
                        initValue,
                        paramGVName
                    );
                    
                    INFO("Created params_config global variable: " << paramGVName << " (scalar, " << dataSize << " bytes)");
                }
                
                _paramsConfigGlobals[id] = {paramGV, dataSize};
            }
        } catch (const YAML::Exception& e) {
            ERROR("Failed to parse params_config.yml: " << e.what());
            abort();
        } catch (const std::exception& e) {
            ERROR("Error loading params_config: " << e.what());
            abort();
        }
    }

    std::map<uint64_t, std::string> kleeEntryBuilder::processPtrValues() {
        std::vector<Region> regions;
        std::map<uint64_t, std::string> addr_region_map;

        // globalVars
        // First collect all globalVars with offset
        for (const auto& var : _modelInputs.globalvars()) {
            int start = var.offset();
            if (start != -1 && !var.name().empty()) {
                regions.emplace_back(var.name(), static_cast<uint32_t>(start), static_cast<uint32_t>(start + var.size()));
            }
        }

        // handle heapVars as heap
        int heapStart = INT_MAX;
        int heapEnd = INT_MIN;
        for (const auto& var : _modelInputs.heapvars()) {
            int start = var.offset();
            int varEnd = start + var.size();
            heapStart = std::min(heapStart, start);
            heapEnd = std::max(heapEnd, varEnd);
        }
        if (heapStart != INT_MAX) {
            regions.push_back({"heap", static_cast<uint32_t>(heapStart), static_cast<uint32_t>(heapEnd)});
        }

        // convert value
        auto processValue = [&](uint64_t value) {
            for (const auto& region : regions) {
                if (value >= region.start && value < region.end) {
                    int rel_offset = value - region.start;
                    std::string region_label = region.name + " + " + std::to_string(rel_offset);
                    addr_region_map[value] = region_label;
                    return;
                }
            }
            for (const auto& constGVEntry : _constGVAddrRanges) {
                uint64_t constStart = constGVEntry.second.first;
                uint64_t constEnd = constGVEntry.second.second;
                if (value >= constStart && value < constEnd) {
                    uint64_t rel_offset = value - constStart;
                    std::string constGVName = constGVEntry.first->getName().str();
                    std::string region_label = constGVName + " + " + std::to_string(rel_offset);
                    addr_region_map[value] = region_label;
                    // INFO("Pointer value " << value << " found in constant GV '" 
                    //         << constGVName << "' at offset " << rel_offset);
                    return;
                }
            }
        };

        // handle ptr in globalVars
        for (auto& var : _modelInputs.globalvars()) {
            if (var.type() == modelInputs::TypeSpec::PTR) {
                processValue(var.i64_value());
            }
            if (var.has_members()) {
                for (const auto& mem : var.members().members()) {
                    if (mem.type() == modelInputs::TypeSpec::PTR) {
                        processValue(mem.i64_value());
                    }
                }
            }
        }

        // handle ptr in heapVars
        for (const auto& var : _modelInputs.heapvars()) {
            if (var.type() == modelInputs::TypeSpec::PTR) {
                processValue(var.i64_value());
            }
            if (var.has_members()) {
                for (const auto& mem : var.members().members()) {
                    if (mem.type() == modelInputs::TypeSpec::PTR) {
                        processValue(mem.i64_value());
                    }
                }
            }
        }
        return addr_region_map;
    }

    std::pair<llvm::GlobalVariable*, uint32_t> kleeEntryBuilder::handlePointToAddr(std::string pointTo){
        if (pointTo.find(" + ") != std::string::npos) {
            size_t pos = pointTo.find(" + ");
            std::string target = pointTo.substr(0, pos);
            std::string offsetStr = pointTo.substr(pos + 3);
            uint32_t offset = std::stoi(offsetStr);
            
            // Check if this is a params_config reference
            if (target == "params_config") {
                auto it = _paramsConfigGlobals.find(offset);
                if (it != _paramsConfigGlobals.end()) {
                    return std::make_pair(it->second.first, 0);
                } else {
                    ERROR("params_config parameter with id " << offset << " not found");
                    abort();
                }
            }
            
            if (target == "heap"){
                return std::make_pair(_heapBytes, offset);
            }else if(_allGlobals.find(target) != _allGlobals.end()){
                return std::make_pair(_allGlobals[target], offset); 
            }else if(_constGlobals.find(target) != _constGlobals.end()){
                llvm::GlobalVariable* constant = _constGlobals[target];
                if (constant == _longestStrConst){
                    return std::make_pair(constant, 0);
                }
                return std::make_pair(constant, offset);
            }
        }
        return std::make_pair(nullptr, 0);
    }

    void kleeEntryBuilder::concretizePointer(llvm::IRBuilder<>& builder, llvm::Module& M, MemLink& link){
        llvm::Value* pointer = builder.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), link.src.gv, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), link.src.offset)});
        llvm::Value* destAddr = builder.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), link.dest.gv, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), link.dest.offset)});
        builder.CreateStore(destAddr, pointer);
    }

    void kleeEntryBuilder::symbolicBasicTyGVs(llvm::IRBuilder<>& builder, llvm::Module& M){
        for (auto& gv : _basicTyGVs){
            llvm::Constant* name = builder.CreateGlobalStringPtr(gv->getName().str());
            builder.CreateCall(_kleeMakeSymbolic, {gv, llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), M.getDataLayout().getTypeAllocSize(gv->getValueType())), name});
        }
    }

    void kleeEntryBuilder::symbolicBasicTyHeapVars(llvm::IRBuilder<>& builder, llvm::Module& M){
        for (auto& heapVar : _basicTyHeapVars){
            uint64_t offset = heapVar.first;
            uint64_t size = heapVar.second;
            std::string nameStr = "heap_" + std::to_string(offset) + "_" + std::to_string(size);
            llvm::Constant* name = builder.CreateGlobalStringPtr(nameStr);
            llvm::Value* ptr = builder.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), _heapBytes, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), offset)});
            builder.CreateCall(_kleeMakeSymbolic, {ptr, llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), size), name});
        }
    }

    void kleeEntryBuilder::symbolicAddrRange(llvm::IRBuilder<>& builder, llvm::Module& M, MemLink& link){
        std::string name = link.src.gv->getName().str() + "_" + std::to_string(link.src.offset) + "_" + std::to_string(link.dest.offset);
        llvm::Constant* const_name = builder.CreateGlobalStringPtr(name);
        llvm::Value* startAddr = builder.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), link.src.gv, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), link.src.offset)});
        uint64_t size = link.dest.offset - link.src.offset;
        // alloca a byte array to store the symbolic value: alloca [size x i8]
        llvm::Value* bytes = builder.CreateAlloca(llvm::ArrayType::get(llvm::Type::getInt8Ty(M.getContext()), size));
        // call klee_make_symbolic
        builder.CreateCall(_kleeMakeSymbolic, {bytes, llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), size), const_name});
        // call memcpy
        builder.CreateCall(_memcpy, {startAddr, bytes, llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), size)});
    }

    void kleeEntryBuilder::bindPtrRelations(llvm::IRBuilder<>& builder, llvm::Module& M){
        for (auto& link : _ptrRelations){
            concretizePointer(builder, M, link);
        }

        // Call printf to print the pointer concretization complete
        llvm::Function* printfF = M.getFunction("printf");
        assert(printfF && "printf function not found");
        std::string printfFormat = "Concretization of pointer members in global variables and heap is complete.\n";
        llvm::Value* format = builder.CreateGlobalStringPtr(printfFormat);
        builder.CreateCall(printfF, {format});
    }

    void kleeEntryBuilder::symbolicAddrRanges(llvm::IRBuilder<>& builder, llvm::Module& M){
        // Check if module name contains "arducopter" or "px4"
        _isArducopter = M.getName().str().find("arducopter") != std::string::npos;
        _isPX4 = M.getName().str().find("px4") != std::string::npos;
        
        // Check if current property has explicitly symbolized hashes
        bool hasPropertySymbolizedHashes = _propertySymbolizedHashes.find(_currentProperty) != _propertySymbolizedHashes.end();
        
        for (auto& link : _addrRangesNeedSymbolized){
            if (_isArducopter) {
                // For arducopter, symbolize "copter" or "heap" (if property has symbolized hashes)
                std::string gvName = link.src.gv->getName().str();
                if (gvName == "copter" || (hasPropertySymbolizedHashes && gvName == "heap"))
                    symbolicAddrRange(builder, M, link);
            } else if (_isPX4) {
                // For PX4, only symbolize "heap"
                if(link.src.gv->getName().str() == "heap")
                    symbolicAddrRange(builder, M, link);
            } else {
                // For other modules, symbolize all
                symbolicAddrRange(builder, M, link);
            }
        }
    }

void kleeEntryBuilder::appendValueToBytes(llvm::Value* target, uint32_t size, uint64_t offset, modelInputs::TypeSpec type, std::string valueStr, llvm::IRBuilder<>& builder, llvm::Module& M) {   

        if (valueStr == "")
            valueStr = std::string("0");
        std::vector<llvm::Value*> args;
        args.push_back(target);
        args.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), offset));

        switch(type) {
            case modelInputs::TypeSpec::INT:{
                if (size == 1){
                    uint8_t val = (uint8_t)std::stoll(valueStr);
                    args.push_back(llvm::ConstantInt::get(llvm::Type::getInt8Ty(M.getContext()), val));
                    builder.CreateCall(_append_i8_to_bytes, args); 
                }else if (size == 2){
                    uint16_t val = (uint16_t)std::stoll(valueStr);
                    args.push_back(llvm::ConstantInt::get(llvm::Type::getInt16Ty(M.getContext()), val));
                    builder.CreateCall(_append_i16_to_bytes, args);
                }else if (size == 4){
                    uint32_t val = (uint32_t)std::stoll(valueStr);
                    args.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(M.getContext()), val));
                    builder.CreateCall(_append_i32_to_bytes, args);
                }else if (size == 8){
                    uint64_t val = (uint64_t)std::stoll(valueStr);
                    args.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), val));
                    builder.CreateCall(_append_i64_to_bytes, args);
                }else{
                    assert(false && "appendValueToBytes: invalid size");
                }
                break;
            }
            case modelInputs::TypeSpec::FLOAT: {
                float val = (float)std::stod(valueStr);
                args.push_back(llvm::ConstantFP::get(llvm::Type::getFloatTy(M.getContext()), val));
                builder.CreateCall(_append_float_to_bytes, args);
                break;
            }
            case modelInputs::TypeSpec::DOUBLE: {
                double val = (double)std::stold(valueStr);
                args.push_back(llvm::ConstantFP::get(llvm::Type::getDoubleTy(M.getContext()), val));
                builder.CreateCall(_append_double_to_bytes, args);
                break;
            }
            default:
                break;
        }
    }

    void kleeEntryBuilder::concretizeGlobalsAndHeap(llvm::IRBuilder<>& builder, llvm::Module &M) {
        for (auto& gv_info : _modelInputs.globalvars()) {
            llvm::GlobalVariable* gv = _allGlobals[gv_info.name()];
            if (gv_info.type() == modelInputs::TypeSpec::AGGR) {
                for (auto& member : gv_info.members().members()) {
                    std::string valStr = "";
                    if (member.type() == modelInputs::TypeSpec::INT)
                        valStr = std::to_string(member.i64_value());
                    else if (member.type() == modelInputs::TypeSpec::FLOAT)
                        valStr = std::to_string(member.f_value());
                    else if (member.type() == modelInputs::TypeSpec::DOUBLE)
                        valStr = std::to_string(member.d_value());

                    appendValueToBytes(gv, member.size(), member.member_offset(), member.type(), valStr, builder, M);
                }
            } else {
                std::string valStr = "";
                if (gv_info.type() == modelInputs::TypeSpec::INT)
                    valStr = std::to_string(gv_info.i64_value());
                else if (gv_info.type() == modelInputs::TypeSpec::FLOAT)
                    valStr = std::to_string(gv_info.f_value());
                else if (gv_info.type() == modelInputs::TypeSpec::DOUBLE)
                    valStr = std::to_string(gv_info.d_value());
                appendValueToBytes(gv, gv_info.size(), 0, gv_info.type(), valStr, builder, M);
            }
        }
    
        for (auto& heap_info : _modelInputs.heapvars()) {
            if (heap_info.type() == modelInputs::TypeSpec::AGGR) {
                for (auto& member : heap_info.members().members()) {
                    uint64_t offset = heap_info.offset() + member.member_offset() - _gvsize;
                    std::string valStr = "";
                    if (member.type() == modelInputs::TypeSpec::INT)
                        valStr = std::to_string(member.i64_value());
                    else if (member.type() == modelInputs::TypeSpec::FLOAT)
                        valStr = std::to_string(member.f_value());
                    else if (member.type() == modelInputs::TypeSpec::DOUBLE)
                        valStr = std::to_string(member.d_value());
                    appendValueToBytes(_heapBytes, member.size(), offset, member.type(), valStr, builder, M);
                }
            } else {
                std::string valStr = "";
                if (heap_info.type() == modelInputs::TypeSpec::INT)
                    valStr = std::to_string(heap_info.i64_value());
                else if (heap_info.type() == modelInputs::TypeSpec::FLOAT)
                    valStr = std::to_string(heap_info.f_value());
                else if (heap_info.type() == modelInputs::TypeSpec::DOUBLE)
                    valStr = std::to_string(heap_info.d_value());
                appendValueToBytes(_heapBytes, heap_info.size(), heap_info.offset() - _gvsize, heap_info.type(), valStr, builder, M);
            }
        }

        // Call printf to print the concretize complete
        llvm::Function* printfF = M.getFunction("printf");
        assert(printfF && "printf function not found");
        std::string printfFormat = "Concretization of basic type members in global variables and heaps is complete.\n";
        llvm::Value* format = builder.CreateGlobalStringPtr(printfFormat);
        builder.CreateCall(printfF, {format});
    }

    void kleeEntryBuilder::callEntryFunction(llvm::IRBuilder<>& builder, llvm::Module& M) {
        // Call printf to print start entry function
        llvm::Function* printfF = M.getFunction("printf");
        assert(printfF && "printf function not found");
        std::string printfFormat = "Call entry function: " + _entryF + "...\n";
        llvm::Value* format = builder.CreateGlobalStringPtr(printfFormat);
        builder.CreateCall(printfF, {format});

        llvm::Function* entryF = M.getFunction(_entryF);
        assert(entryF && "entry function not found");
        // handle parameter, if it is ptr type, then handle it by offset. For example, copter + 32152
        std::vector<llvm::Value*> args;
        for (auto& param : _paramList){
            std::string paramType = param.second.first;
            llvm::Value* arg;
            if (paramType == "ptr"){
                std::string pointTo = param.second.second;
                if (pointTo.find(" + ") != std::string::npos) {
                    size_t pos = pointTo.find(" + ");
                    std::string target = pointTo.substr(0, pos);
                    std::string offsetStr = pointTo.substr(pos + 3);
                    uint32_t offset = std::stoi(offsetStr);
                    
                    // Check if this is a params_config reference
                    if (target == "params_config") {
                        auto it = _paramsConfigGlobals.find(offset);
                        if (it != _paramsConfigGlobals.end()) {
                            arg = builder.CreateBitCast(it->second.first, llvm::Type::getInt8PtrTy(M.getContext()));
                        } else {
                            ERROR("params_config parameter with id " << offset << " not found");
                            abort();
                        }
                    } else if (target == "heap") {
                        arg = builder.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), _heapBytes, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), offset)});
                    } else {
                        llvm::GlobalVariable* targetGV = _allGlobals[target];
                        arg = builder.CreateGEP(llvm::Type::getInt8Ty(M.getContext()), targetGV, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), offset)});
                    }
                }
            } else{
                std::string value = param.second.second;
                if (paramType == "i1"){
                    arg = llvm::ConstantInt::get(llvm::Type::getInt1Ty(M.getContext()), std::stoi(value));
                }else if (paramType == "i8"){
                    arg = llvm::ConstantInt::get(llvm::Type::getInt8Ty(M.getContext()), std::stoi(value));
                }else if (paramType == "i16"){
                    arg = llvm::ConstantInt::get(llvm::Type::getInt16Ty(M.getContext()), std::stoi(value));
                }else if (paramType == "i32"){
                    arg = llvm::ConstantInt::get(llvm::Type::getInt32Ty(M.getContext()), std::stoi(value));
                }else if (paramType == "i64"){
                    arg = llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), std::stoll(value));
                }else if (paramType == "float"){
                    arg = llvm::ConstantFP::get(llvm::Type::getFloatTy(M.getContext()), std::stof(value));
                }else if (paramType == "double"){
                    arg = llvm::ConstantFP::get(llvm::Type::getDoubleTy(M.getContext()), std::stod(value));
                }else {
                    assert(false && "Error: Unsupported parameter type");
                }
            }
            args.push_back(arg);
        }
        // Create the call instruction and set the calling convention to match the target function
        llvm::CallInst* callInst = builder.CreateCall(entryF, args);
        callInst->setCallingConv(entryF->getCallingConv());
    }

    void kleeEntryBuilder::analysisGVs(llvm::Module &M) {
        // Check module type at the beginning
        _isArducopter = M.getName().str().find("arducopter") != std::string::npos;
        _isPX4 = M.getName().str().find("px4") != std::string::npos;
        
        bool hasDeclareSymbolic = false;

        if(_addrRangesNeedSymbolized.empty())
            hasDeclareSymbolic = true;

        std::map<uint64_t, std::string> addr_region_map = processPtrValues();

        for (auto& gv_info : _modelInputs.globalvars()) {
            if (gv_info.type() == modelInputs::TypeSpec::PTR){
                llvm::GlobalVariable* srcGV = _allGlobals[gv_info.name()];
                std::pair<llvm::GlobalVariable*, uint32_t> dest = handlePointToAddr(addr_region_map[gv_info.i64_value()]);
                if (dest.first != nullptr){
                    _ptrRelations.push_back(MemLink(GVAddrInfo(srcGV, 0), GVAddrInfo(dest.first, dest.second)));
                }
            }else if (gv_info.type() == modelInputs::TypeSpec::AGGR){
                // Step 1: Collect all pointer ranges and handle pointer relations
                std::vector<std::pair<uint64_t, uint64_t>> skips;
                for (const auto& member : gv_info.members().members()) {
                    if (member.type() == modelInputs::TypeSpec::PTR) {
                        uint64_t ptr_start = member.member_offset();
                        uint64_t ptr_end = ptr_start + member.size();
                        skips.push_back(std::make_pair(ptr_start, ptr_end));

                        // Handle pointer relation
                        std::pair<llvm::GlobalVariable*, uint32_t> dest = handlePointToAddr(addr_region_map[member.i64_value()]);
                        if (dest.first != nullptr) {
                            _ptrRelations.push_back(MemLink(
                                GVAddrInfo(_allGlobals[gv_info.name()], ptr_start),
                                GVAddrInfo(dest.first, dest.second)
                            ));
                        }
                    }
                }
                // Step 2: Compute non-pointer intervals separated by skips
                if (hasDeclareSymbolic) {
                    uint64_t total_size = gv_info.size();
                    uint64_t current_pos = 0;
                    for (const auto& ptr : skips) {
                        uint64_t ptr_start = ptr.first;
                        if (current_pos < ptr_start) {
                            _addrRangesNeedSymbolized.push_back(MemLink(
                                GVAddrInfo(_allGlobals[gv_info.name()], current_pos),
                                GVAddrInfo(_allGlobals[gv_info.name()], ptr_start)
                            ));
                        }
                        current_pos = ptr.second;  // Skip pointer
                    }

                    // Add tail interval
                    if (current_pos < total_size) {
                        _addrRangesNeedSymbolized.push_back(MemLink(
                            GVAddrInfo(_allGlobals[gv_info.name()], current_pos),
                            GVAddrInfo(_allGlobals[gv_info.name()], total_size)
                        ));
                    }
                }
            }else{
                _basicTyGVs.push_back(_allGlobals[gv_info.name()]);
            }
        }

        // handle heap pointer - treat heap as a whole
        // Step 1: Collect all pointer ranges and skipped ranges across the entire heap
        std::vector<std::pair<uint64_t, uint64_t>> heapPtrRanges;
        std::vector<std::pair<uint64_t, uint64_t>> heapSkippedRanges;
        std::vector<std::pair<uint64_t, uint64_t>> heapSymbolizedRanges; // Ranges to be explicitly symbolized for the current property
        
        // Check if current property has symbolized hashes
        auto propertyHashIt = _propertySymbolizedHashes.find(_currentProperty);
        bool hasPropertySymbolizedHashes = (propertyHashIt != _propertySymbolizedHashes.end());
        
        for(auto& heap_info : _modelInputs.heapvars()){
            if (heap_info.type() == modelInputs::TypeSpec::PTR){
                // Single pointer variable
                uint64_t ptr_start = heap_info.offset() - _gvsize;
                uint64_t ptr_end = ptr_start + heap_info.size();
                heapPtrRanges.push_back(std::make_pair(ptr_start, ptr_end));
                
                std::pair<llvm::GlobalVariable*, uint32_t> dest = handlePointToAddr(addr_region_map[heap_info.i64_value()]);
                if (dest.first != nullptr){
                    _ptrRelations.push_back(MemLink(GVAddrInfo(_heapBytes, ptr_start), GVAddrInfo(dest.first, dest.second)));
                }
            }else if (heap_info.type() == modelInputs::TypeSpec::AGGR){
                // Get hash and base offset for this heap variable
                uint64_t heap_hash = heap_info.hash();
                uint64_t heap_base_offset = heap_info.offset() - _gvsize;
                
                // Aggregate type - collect pointer members
                for (auto& member : heap_info.members().members()){
                    if (member.type() == modelInputs::TypeSpec::PTR){
                        uint64_t ptr_start = heap_info.offset() + member.member_offset() - _gvsize;
                        uint64_t ptr_end = ptr_start + member.size();
                        heapPtrRanges.push_back(std::make_pair(ptr_start, ptr_end));

                        std::pair<llvm::GlobalVariable*, uint32_t> dest = handlePointToAddr(addr_region_map[member.i64_value()]);
                        if (dest.first != nullptr){
                            _ptrRelations.push_back(MemLink(
                                GVAddrInfo(_heapBytes, ptr_start), 
                                GVAddrInfo(dest.first, dest.second)
                            ));
                        }
                    }
                }
                
                // Check if this heap variable should be symbolized for the current property
                if (hasPropertySymbolizedHashes) {
                    for (const auto& symbolizedHash : propertyHashIt->second) {
                        if (symbolizedHash == heap_hash) {
                            // Automatically use the entire range of this heap variable
                            uint64_t abs_start = heap_base_offset;
                            uint64_t abs_end = heap_base_offset + heap_info.size();
                            heapSymbolizedRanges.push_back(std::make_pair(abs_start, abs_end));
                            // INFO("Found symbolized hash " << heap_hash << " for property " << _currentProperty 
                            //      << ": heap variable size " << heap_info.size() 
                            //      << ", absolute range [" << abs_start << ", " << abs_end << ")");
                            break; // Found the hash, no need to continue
                        }
                    }
                }
                
                // Check if this heap variable matches any skipped range by hash
                for (const auto& skippedRange : _skippedHeapRanges) {
                    if (skippedRange.hash == heap_hash) {
                        // Convert relative offset (within the AGGR variable) to absolute offset (within heap)
                        uint64_t skip_start = heap_base_offset + skippedRange.startIndex;
                        uint64_t skip_end = heap_base_offset + skippedRange.endIndex;
                        heapSkippedRanges.push_back(std::make_pair(skip_start, skip_end));
                        // INFO("Found skipped range for heap variable with hash " << heap_hash 
                        //      << ": relative [" << skippedRange.startIndex << ", " << skippedRange.endIndex 
                        //      << ") -> absolute [" << skip_start << ", " << skip_end << ")");
                    }
                }
            }else{
                // Basic type heap variable
                _basicTyHeapVars.push_back(std::make_pair(heap_info.offset() - _gvsize, heap_info.size()));
            }
        }
        
        // Step 2: Sort and merge pointer ranges and skipped ranges, then compute non-pointer intervals for the entire heap
        if (hasDeclareSymbolic && !_isArducopter && !heapPtrRanges.empty()) {
            // Merge pointer ranges and skipped ranges together as they both should not be symbolized
            std::vector<std::pair<uint64_t, uint64_t>> allSkippedRanges;
            allSkippedRanges.insert(allSkippedRanges.end(), heapPtrRanges.begin(), heapPtrRanges.end());
            allSkippedRanges.insert(allSkippedRanges.end(), heapSkippedRanges.begin(), heapSkippedRanges.end());
            
            // Sort by start address
            std::sort(allSkippedRanges.begin(), allSkippedRanges.end());
            
            // Merge overlapping ranges
            std::vector<std::pair<uint64_t, uint64_t>> mergedSkippedRanges;
            mergedSkippedRanges.push_back(allSkippedRanges[0]);
            for (size_t i = 1; i < allSkippedRanges.size(); ++i) {
                auto& last = mergedSkippedRanges.back();
                auto& curr = allSkippedRanges[i];
                if (curr.first <= last.second) {
                    // Overlapping or adjacent, merge them
                    last.second = std::max(last.second, curr.second);
                } else {
                    // Non-overlapping, add as new range
                    mergedSkippedRanges.push_back(curr);
                }
            }
            
            // Compute symbolizable intervals across the entire heap (excluding both pointers and skipped ranges)
            uint64_t current_pos = 0;
            for (const auto& skippedRange : mergedSkippedRanges) {
                uint64_t skip_start = skippedRange.first;
                uint64_t skip_end = skippedRange.second;
                
                if (current_pos < skip_start) {
                    // Add symbolizable interval before this skipped range
                    _addrRangesNeedSymbolized.push_back(MemLink(
                        GVAddrInfo(_heapBytes, current_pos),
                        GVAddrInfo(_heapBytes, skip_start)
                    ));
                }
                current_pos = skip_end;
            }
            
            // Add tail interval if there's symbolizable data after the last skipped range
            if (current_pos < _hsize) {
                _addrRangesNeedSymbolized.push_back(MemLink(
                    GVAddrInfo(_heapBytes, current_pos),
                    GVAddrInfo(_heapBytes, _hsize)
                ));
            }
        }
        
        // Step 3: Add explicitly symbolized ranges for the current property
        // These ranges should be symbolized regardless of other settings
        if (hasPropertySymbolizedHashes && !heapSymbolizedRanges.empty()) {
            // Sort and merge symbolized ranges
            std::sort(heapSymbolizedRanges.begin(), heapSymbolizedRanges.end());
            
            std::vector<std::pair<uint64_t, uint64_t>> mergedSymbolizedRanges;
            mergedSymbolizedRanges.push_back(heapSymbolizedRanges[0]);
            for (size_t i = 1; i < heapSymbolizedRanges.size(); ++i) {
                auto& last = mergedSymbolizedRanges.back();
                auto& curr = heapSymbolizedRanges[i];
                if (curr.first <= last.second) {
                    last.second = std::max(last.second, curr.second);
                } else {
                    mergedSymbolizedRanges.push_back(curr);
                }
            }
            
            // For each symbolized range, we need to exclude pointers and skipped ranges within it and add the rest
            for (const auto& symRange : mergedSymbolizedRanges) {
                uint64_t sym_start = symRange.first;
                uint64_t sym_end = symRange.second;
                // INFO("Processing merged symbolized range [" << sym_start << ", " << sym_end << ")");
                
                // Find all pointer ranges and skipped ranges that intersect with this symbolized range
                std::vector<std::pair<uint64_t, uint64_t>> excludedRanges;
                
                // Add intersecting pointer ranges
                for (const auto& ptrRange : heapPtrRanges) {
                    if (ptrRange.first < sym_end && ptrRange.second > sym_start) {
                        uint64_t inter_start = std::max(ptrRange.first, sym_start);
                        uint64_t inter_end = std::min(ptrRange.second, sym_end);
                        excludedRanges.push_back(std::make_pair(inter_start, inter_end));
                    }
                }
                
                // Add intersecting skipped ranges
                for (const auto& skipRange : heapSkippedRanges) {
                    if (skipRange.first < sym_end && skipRange.second > sym_start) {
                        uint64_t inter_start = std::max(skipRange.first, sym_start);
                        uint64_t inter_end = std::min(skipRange.second, sym_end);
                        excludedRanges.push_back(std::make_pair(inter_start, inter_end));
                    }
                }
                
                // INFO("Found " << excludedRanges.size() << " excluded ranges (pointers + skipped)");
                
                if (excludedRanges.empty()) {
                    // No excluded ranges in this symbolized range, symbolize the entire range
                    // INFO("No excluded ranges, symbolizing entire range [" << sym_start << ", " << sym_end << ")");
                    _addrRangesNeedSymbolized.push_back(MemLink(
                        GVAddrInfo(_heapBytes, sym_start),
                        GVAddrInfo(_heapBytes, sym_end)
                    ));
                } else {
                    // Sort and merge excluded ranges
                    std::sort(excludedRanges.begin(), excludedRanges.end());
                    
                    std::vector<std::pair<uint64_t, uint64_t>> mergedExcludedRanges;
                    mergedExcludedRanges.push_back(excludedRanges[0]);
                    for (size_t i = 1; i < excludedRanges.size(); ++i) {
                        auto& last = mergedExcludedRanges.back();
                        auto& curr = excludedRanges[i];
                        if (curr.first <= last.second) {
                            last.second = std::max(last.second, curr.second);
                        } else {
                            mergedExcludedRanges.push_back(curr);
                        }
                    }
                    
                    // INFO("Merged excluded ranges:");
                    // for (const auto& range : mergedExcludedRanges) {
                    //     INFO("  Excluded range [" << range.first << ", " << range.second << ")");
                    // }
                    
                    // Symbolize gaps between excluded ranges
                    uint64_t current = sym_start;
                    for (const auto& excludedRange : mergedExcludedRanges) {
                        if (current < excludedRange.first) {
                            // INFO("Adding symbolization range [" << current << ", " << excludedRange.first << ")");
                            _addrRangesNeedSymbolized.push_back(MemLink(
                                GVAddrInfo(_heapBytes, current),
                                GVAddrInfo(_heapBytes, excludedRange.first)
                            ));
                        }
                        current = std::max(current, excludedRange.second);
                    }
                    
                    // Add tail if needed
                    if (current < sym_end) {
                        // INFO("Adding tail symbolization range [" << current << ", " << sym_end << ")");
                        _addrRangesNeedSymbolized.push_back(MemLink(
                            GVAddrInfo(_heapBytes, current),
                            GVAddrInfo(_heapBytes, sym_end)
                        ));
                    }
                }
            }
        }
    }

    void kleeEntryBuilder::parseELFFile(llvm::Module &M){
        // Use LLVM's Object library to parse ELF file
        llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> bufferOrErr = 
            llvm::MemoryBuffer::getFile(_elfFile);
        
        if (std::error_code ec = bufferOrErr.getError()) {
            std::cerr << "Warning: Failed to open ELF file: " << _elfFile 
                     << " - " << ec.message() << "\n";
            return;
        }

        llvm::Expected<std::unique_ptr<llvm::object::ObjectFile>> objOrErr = 
            llvm::object::ObjectFile::createObjectFile(bufferOrErr.get()->getMemBufferRef());
        
        if (!objOrErr) {
            std::cerr << "Warning: Failed to parse ELF file: " << _elfFile << "\n";
            llvm::consumeError(objOrErr.takeError());
            return;
        }

        llvm::object::ObjectFile* objFile = objOrErr.get().get();
        
        // Check if it's an ELF file
        if (!objFile->isELF()) {
            std::cerr << "Warning: File is not an ELF file: " << _elfFile << "\n";
            return;
        }

        // Build a map of symbol names to their addresses and sizes
        std::map<std::string, std::pair<uint64_t, uint64_t>> symbolMap;
        
        // Iterate through all symbols
        for (const auto& symbolRef : objFile->symbols()) {
            llvm::Expected<llvm::StringRef> nameOrErr = symbolRef.getName();
            if (!nameOrErr) {
                llvm::consumeError(nameOrErr.takeError());
                continue;
            }
            
            llvm::StringRef name = nameOrErr.get();
            if (name.empty()) continue;

            // Get symbol type
            llvm::Expected<llvm::object::SymbolRef::Type> typeOrErr = symbolRef.getType();
            if (!typeOrErr) {
                llvm::consumeError(typeOrErr.takeError());
                continue;
            }

            // Only consider data symbols (OBJECT type)
            if (typeOrErr.get() != llvm::object::SymbolRef::ST_Data) {
                continue;
            }

            // Get symbol address
            llvm::Expected<uint64_t> addrOrErr = symbolRef.getAddress();
            if (!addrOrErr) {
                llvm::consumeError(addrOrErr.takeError());
                continue;
            }
            uint64_t addr = addrOrErr.get();

            // Get symbol size
            uint64_t size = llvm::object::ELFSymbolRef(symbolRef).getSize();
            
            if (size == 0) continue;

            symbolMap[name.str()] = std::make_pair(addr, addr + size);
        }

        size_t symbolCount = symbolMap.size();
        size_t constantGVCount = 0;
        size_t longest_str_len = 0;
        size_t total_str_len = 0;
        size_t stringCount = 0;
        // Match IR constant global variables with ELF symbols
        for (auto& GV : M.globals()) {
            if (!GV.isConstant()) continue;
            constantGVCount++;
            
            std::string gvName = GV.getName().str();
            auto it = symbolMap.find(gvName);
            if (it != symbolMap.end()) {
                _constGVAddrRanges[&GV] = std::make_pair(it->second.first, it->second.second);
                // std::cout << "Found constant variable '" << gvName 
                //          << "' at address range [0x" << std::hex << it->second.first
                //          << ", 0x" << it->second.second << std::dec << ")\n";
            }else{
                // Check if this is a string constant and track the longest one
                if (GV.hasInitializer()) {
                    if (auto* constData = llvm::dyn_cast<llvm::ConstantDataSequential>(GV.getInitializer())) {
                        if (constData->isString()) {
                            size_t str_len = constData->getAsString().size();
                            if (str_len > longest_str_len) {
                                longest_str_len = str_len;
                                _longestStrConst = &GV;
                            }
                            total_str_len += str_len;
                            stringCount++;
                        }else{
                            // Not a string constant
                            LLVM_WARNING("Constant global variable '" << GV
                                         << "' not found in ELF symbols and is not a string constant.");
                        }
                    }
                }
            }
        }
        
        // Print information about the longest string constant found
        if (_longestStrConst) {
            INFO("Found longest string constant: '" << _longestStrConst->getName().str() 
                      << "' with length " << longest_str_len);
            INFO("Total length of all string constants: " << total_str_len);
        }
        
        // Get strings section address range
        std::pair<uint32_t, uint32_t> stringsAddrRange = std::make_pair(0, 0);
        for (const auto& section : objFile->sections()) {
            llvm::Expected<llvm::StringRef> nameOrErr = section.getName();
            if (!nameOrErr) {
                llvm::consumeError(nameOrErr.takeError());
                continue;
            }
            
            llvm::StringRef sectionName = nameOrErr.get();
            if (sectionName == "strings") {
                uint64_t startAddr = section.getAddress();
                uint64_t size = section.getSize();
                uint64_t endAddr = startAddr + size;
                stringsAddrRange = std::make_pair(static_cast<uint32_t>(startAddr), 
                                                 static_cast<uint32_t>(endAddr));
                INFO("Found strings section at address range [" << startAddr
                          << ", " << endAddr << ")");
                break;
            }
        }

        _constGVAddrRanges[_longestStrConst] = stringsAddrRange;
        // Create metadata for constant global variables to store address ranges
        for (const auto& entry : _constGVAddrRanges) {
            llvm::GlobalVariable* gv = entry.first;
            uint64_t startAddr = entry.second.first;
            uint64_t endAddr = entry.second.second;
            llvm::LLVMContext& context = M.getContext();
            llvm::Metadata* startMD = llvm::ConstantAsMetadata::get(
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), startAddr));
            llvm::Metadata* endMD = llvm::ConstantAsMetadata::get(
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), endAddr));
            llvm::MDNode* addrRangeNode = llvm::MDNode::get(context, {startMD, endMD});
            gv->setMetadata("constAddrRange", addrRangeNode);
            _constGlobals[gv->getName().str()] = gv;
        }
        INFO("Parsed " << _constGVAddrRanges.size() - 1
                  << " constant global variables from ELF file.");
        INFO("Parsed " << stringCount
                  << " string constants from ELF file.");
        INFO("Total symbols in ELF: " << symbolCount
                  << ", global constants in IR: " << constantGVCount);
    }

    void kleeEntryBuilder::createKleeEntry(llvm::Module &M){
        llvm::FunctionType* kleeEntryType = llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), false);
        llvm::Function* kleeEntry = llvm::Function::Create(kleeEntryType, llvm::GlobalValue::ExternalLinkage, "klee_entry", M);
        llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(M.getContext(), "entry", kleeEntry);
        llvm::IRBuilder<> builder(entryBB);


        analysisGVs(M);
        concretizeGlobalsAndHeap(builder, M);
        bindPtrRelations(builder, M);

        // Call printf to print Symbolic start
        llvm::Function* printfF = M.getFunction("printf");
        assert(printfF && "printf function not found");
        std::string printfFormat = "Start symbolizing some of the content...\n";
        llvm::Value* format = builder.CreateGlobalStringPtr(printfFormat);
        builder.CreateCall(printfF, {format});

        // symbolicBasicTyGVs(builder, M);
        // symbolicBasicTyHeapVars(builder, M);

        symbolicAddrRanges(builder, M);

        callEntryFunction(builder, M);

        builder.CreateRetVoid();
    }
    
    // remove call @llvm.type.test and @llvm.assume
    void removeTypeTestAndAssume(llvm::Module &M){
        for (auto& F : M){
            for (auto& BB : F){
                for (auto I = BB.begin(), E = BB.end(); I != E; ) {
                    if (auto* call = llvm::dyn_cast<llvm::CallBase>(&*I)){
                        if (!call->isIndirectCall()){
                            if (call->getCalledFunction()->getName() == "llvm.type.test" || call->getCalledFunction()->getName() == "llvm.assume"){
                                I = call->eraseFromParent();
                                continue;
                            }
                        }
                    }
                    ++I;
                }
            }
        }
    }

    // Simplify complex functions that cause KLEE solver issues
    // Replace them with klee_silent_exit() to silently terminate the path without generating test cases
    void simplifyComplexFunctions(llvm::Module &M){
        llvm::LLVMContext& ctx = M.getContext();
        
        // Functions that should silently terminate the execution path
        std::vector<std::string> functionsToAbort = {
            "_ZN6AP_HAL5panicEPKcz",                                          // AP_HAL::panic(const char*, ...)
            "_ZN6AP_HAL16dump_stack_traceEv",                                // AP_HAL::dump_stack_trace()
            "_ZN6AP_HALL21run_command_on_ownpidEPKc",                        // AP_HAL::run_command_on_ownpid
        };
        
        // Functions to completely stub out (replace with empty implementation that just returns)
        // Map of function names to their return values
        static const std::map<std::string, int> STUB_FUNCTIONS = {
            {"_ZN9AP_Logger11Write_ErrorE17LogErrorSubsystem12LogErrorCode", 0},  // AP_Logger::Write_Error
            {"_ZN9AP_Logger18WriteCriticalBlockEPKvt", 0},                        // AP_Logger::WriteCriticalBlock
            {"_ZN17AP_Logger_Backend18WriteCriticalBlockEPKvt", 0},               // AP_Logger_Backend::WriteCriticalBlock
            {"_ZN17AP_Logger_Backend21WritePrioritisedBlockEPKvtbb", 0},          // AP_Logger_Backend::WritePrioritisedBlock
            {"_ZN17AP_Logger_Backend30validate_WritePrioritisedBlockEPKvt", 0},   // AP_Logger_Backend::validate_WritePrioritisedBlock
            {"_ZN3GCS10send_textvE12MAV_SEVERITYPKcSt9__va_listh", 0},             // GCS::send_text(MAV_SEVERITY)
            {"px4_log_modulename", 0},
            {"mavlink_vasprintf", 0},
            {"_ZN9AP_Logger17set_vehicle_armedEb", 0},                             // AP_Logger::set_vehicle_armed
            {"_Z13print_vprintfPN6AP_HAL12BetterStreamEPKcSt9__va_list", 0},                    // print_vprintf(AP_HAL::BetterStream*, const char*, __va_list)
        };
        
        // Find or create klee_silent_exit declaration
        llvm::Function* kleeSilentExit = M.getFunction("klee_silent_exit");
        if (!kleeSilentExit) {
            llvm::FunctionType* kleeSilentExitType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(ctx), 
                {llvm::Type::getInt32Ty(ctx)}, // exit code parameter
                false);
            kleeSilentExit = llvm::Function::Create(
                kleeSilentExitType, 
                llvm::GlobalValue::ExternalLinkage, 
                "klee_silent_exit", 
                M);
        }
        
        // Replace functions that should abort with klee_silent_exit(0)
        for (const std::string& funcName : functionsToAbort) {
            llvm::Function* targetFunc = M.getFunction(funcName);
            if (!targetFunc || targetFunc->isDeclaration()) {
                continue;
            }
            
            // INFO("Replacing function with klee_silent_exit: " << funcName);
            
            // Clear all basic blocks in the function
            while (!targetFunc->empty()) {
                targetFunc->back().eraseFromParent();
            }
            
            // Create a new entry block that calls klee_silent_exit
            llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(
                ctx, "entry", targetFunc);
            llvm::IRBuilder<> builder(entryBB);
            
            // Call klee_silent_exit(0) to silently terminate this path
            builder.CreateCall(kleeSilentExit, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0)});
            builder.CreateUnreachable();
        }
        
        // Replace logger functions with simple stubs that just return
        for (const auto& [funcName, returnValue] : STUB_FUNCTIONS) {
            llvm::Function* targetFunc = M.getFunction(funcName);
            if (!targetFunc || targetFunc->isDeclaration()) {
                continue;
            }
            
            // INFO("Stubbing out function: " << funcName);
            
            // Clear all basic blocks in the function
            while (!targetFunc->empty()) {
                targetFunc->back().eraseFromParent();
            }
            
            // Create a new simple entry block
            llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(
                ctx, "entry", targetFunc);
            llvm::IRBuilder<> builder(entryBB);
            
            // Just return (with appropriate return value if needed)
            llvm::Type* retType = targetFunc->getReturnType();
            if (retType->isVoidTy()) {
                builder.CreateRetVoid();
            } else if (retType->isIntegerTy()) {
                builder.CreateRet(llvm::ConstantInt::get(retType, returnValue));
            } else if (retType->isFloatingPointTy()) {
                builder.CreateRet(llvm::ConstantFP::get(retType, static_cast<double>(returnValue)));
            } else if (retType->isPointerTy()) {
                builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(retType)));
            } else {
                builder.CreateUnreachable();
            }
        }
    }

    void runKleeEntryBuilder(llvm::Module &M, std::string pbPath, std::string entryF, std::map<int, std::pair<std::string, std::string>> paramList, std::string elfFile, std::string currentProperty){
        // xml::parseXML(xmlPath);
        // xml::processPtrValues();

        llvm::PassBuilder PB;
        llvm::ModulePassManager MPM;
        llvm::ModuleAnalysisManager MAM;

        // Register analysis passes with the managers
        PB.registerModuleAnalyses(MAM);
        MPM.addPass(kleeEntryBuilder(entryF, pbPath, paramList, elfFile, currentProperty));
        MPM.run(M, MAM);

        // if(llvm::StripDebugInfo(M))
        //     llvm::outs() << "Strip debug info.\n";
        removeTypeTestAndAssume(M);
        INFO("Remove type test and assume.");
        
        // Simplify complex functions to avoid KLEE solver issues
        simplifyComplexFunctions(M);
        INFO("Simplified complex functions (panic, dump_stack_trace, etc.).");
        
        M.setTargetTriple("x86_64-unknown-linux-gnu");
        M.setDataLayout("e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128");
    }

    
}