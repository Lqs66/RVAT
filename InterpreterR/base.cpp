#include "base/base.h"
#include "base/exprtk.hpp"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Constants.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/IR/IRBuilder.h"
#include "model/CFAModelFactory.h"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <chrono>
#include <iomanip>

static IRConfig _config;
static std::vector<std::unique_ptr<uppllvm::CFAModel>> _CFAModels; // All CFAModels built from the target functions
static uppllvm::CFAModel* _entryPointCFAModel = nullptr; // The CFAModel of the entry point function
static std::string _dtmc = utils::getEnv("DTMC");
static std::vector<uppllvm::GenericValue> _entryArgs; // The arguments of the entry point function
static std::unique_ptr<llvm::LLVMContext> GlobalContext; // Global LLVMContext - must outlive the Module and ExecutionEngine
static llvm::Module* GlobalModule = nullptr; // Global Module pointer - for easy access throughout the program
static std::unordered_map<std::string, llvm::Value*> _registerVarNeedsQuery; // Register variables that need to be queried during interpretation
static std::unordered_map<std::string, std::pair<llvm::AllocaInst*, uint64_t>> _stackVarNeedsQuery; // Store llvm alloca inst with llvm dbg intrinsic
static std::unordered_map<std::string, std::pair<llvm::GlobalVariable*, uint64_t>> _globalVarNeedsQuery; // Global variables that need to be queried during interpretation
static std::unordered_map<std::string, llvm::Type*> _pVarTypes; // Types of property variables

// Storage for params_config data: maps property name to (data pointer, data size)
static std::unordered_map<std::string, std::pair<void*, size_t>> _paramsConfigData;

#ifdef DEBUG
static modelInputs::ModelInputs DebugModelInputs; // Store initialized memory information for debug output
#endif

// Store constant global variables and their address ranges parsed from ELF file
static std::map<llvm::GlobalVariable*, std::pair<uint64_t, uint64_t>> _constGVAddrRanges;
static llvm::GlobalVariable* _longestStrConst = nullptr; // The longest string constant in IR

// Union type override configuration structure for handling PX4 union issues
struct HeapVarUnionOverride {
    uint64_t hash;
    uint64_t startOffset;
    uint64_t endOffset;
    std::unordered_map<std::string, std::string> realType; // property_name -> actual_type_name
};

// Global storage for union overrides loaded from YAML config
static std::vector<HeapVarUnionOverride> _heapVarUnionOverrides;

/**
 * @brief Get the real struct type for a heap variable based on union override configuration.
 * 
 * This function resolves the actual runtime type for union members by consulting the
 * union override configuration. When a union contains multiple possible types (e.g.,
 * FlightTaskAutoFollowTarget, FlightTaskOrbit), the MI file may store it as one type
 * but the actual runtime type differs. This function returns the correct type.
 * 
 * @param unionOverride Pointer to union override configuration (can be nullptr)
 * @param property_name Current property name being verified
 * @param Context LLVM context for type lookup
 * @return The real struct type, or nullptr if no override or type not found
 */
static llvm::StructType* getRealStructType(
    const HeapVarUnionOverride* unionOverride,
    const std::string& property_name,
    llvm::LLVMContext& Context) {
    
    if (!unionOverride) {
        return nullptr;
    }
    
    // Look up the real type name for this property
    auto it = unionOverride->realType.find(property_name);
    if (it == unionOverride->realType.end()) {
        return nullptr;
    }
    
    std::string realTypeName = it->second;
    llvm::StructType* realType = llvm::StructType::getTypeByName(Context, realTypeName);
    
    if (!realType) {
        llvm::errs() << "[WARNING] Real struct type '" << realTypeName 
                     << "' not found in module for property '" << property_name << "'\n";
    }
    
    return realType;
}

/**
 * @brief Stub out specified functions by replacing their body with a simple ret instruction.
 * 
 * This function modifies the IR to replace the body of specified functions with just a return
 * instruction. This is useful for functions like vsnprintf that:
 * 1. Are complex to analyze (e.g., variadic arguments)
 * 2. Have side effects we don't care about during verification
 * 3. Would be treated as external/out-of-inline-depth anyway
 * 
 * By stubbing them at the IR level, we avoid:
 * - Complex CFA construction for these functions
 * - Need for special handling during interpretation
 * - They can be normally inlined but do nothing
 * 
 * @param M The LLVM Module to process
 */
static void stubOutFunctions(llvm::Module& M) {
    for (const auto& entry : STUB_FUNCTIONS) {
        const std::string& funcName = entry.first;
        int returnValue = entry.second;
        
        llvm::Function* F = M.getFunction(funcName);
        if (!F) {
            continue; // Function not found, skip
        }
        
        if (F->isDeclaration()) {
            continue; // Already a declaration, nothing to stub
        }
        
        // Clear the function body
        F->deleteBody();
        
        // Create a new basic block with just a return instruction
        llvm::BasicBlock* BB = llvm::BasicBlock::Create(M.getContext(), "entry", F);
        llvm::IRBuilder<> Builder(BB);
        
        // Create appropriate return instruction based on return type
        llvm::Type* RetTy = F->getReturnType();
        if (RetTy->isVoidTy()) {
            Builder.CreateRetVoid();
        } else if (RetTy->isIntegerTy()) {
            // Return the specified value for integer types
            Builder.CreateRet(llvm::ConstantInt::get(RetTy, returnValue));
        } else if (RetTy->isFloatingPointTy()) {
            // Return the specified value (cast to double) for floating point types
            Builder.CreateRet(llvm::ConstantFP::get(RetTy, static_cast<double>(returnValue)));
        } else if (RetTy->isPointerTy()) {
            // For pointer types, return null (ignore returnValue)
            Builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(RetTy)));
        } else {
            // For other types, try to create a zero constant
            Builder.CreateRet(llvm::Constant::getNullValue(RetTy));
        }
        
        // INFO("Stubbed function: " << funcName << " with return value: " << returnValue);
    }
}

// Load union override configuration from YAML file
static void loadUnionOverrideConfig(const std::string& platform) {
    _heapVarUnionOverrides.clear();
    if (platform == "px4") {
        // Construct config file path
        std::string configPath = _dtmc + "/InterpreterR/base/px4_union_config.yml";
        
        if (!std::filesystem::exists(configPath)) {
            ERROR("No union config file found at: " << configPath << "\n");
            abort();
        }
        
        try {
            YAML::Node config = YAML::LoadFile(configPath);
            
            if (!config["HeapVars"]) {
                ERROR("No HeapVars defined in union config\n");
                abort();
            }
            
            for (const auto& heapVar : config["HeapVars"]) {
                HeapVarUnionOverride override;
                override.hash = heapVar["Hash"].as<uint64_t>();
                override.startOffset = heapVar["StartOffset"].as<uint64_t>();
                override.endOffset = heapVar["EndOffset"].as<uint64_t>();
                
                if (heapVar["realType"]) {
                    for (const auto& type : heapVar["realType"]) {
                        std::string propertyName = type.first.as<std::string>();
                        std::string typeName = type.second.as<std::string>();
                        override.realType[propertyName] = typeName;
                    }
                }
                _heapVarUnionOverrides.push_back(override);
            }
        } catch (const YAML::Exception& e) {
            ERROR("Failed to parse union config: " << e.what() << "\n");
        }
    }
}

// Helper function to check if an intrinsic is supported by IntrinsicLowering
// Based on the switch statement in IntrinsicLowering::LowerIntrinsicCall
// This matches the LLVM 16.0.4 version of IntrinsicLowering
static bool isSupportedIntrinsic(llvm::Intrinsic::ID ID) {
    switch (ID) {
    // Supported intrinsics (from IntrinsicLowering::LowerIntrinsicCall)
    case llvm::Intrinsic::expect:
    case llvm::Intrinsic::ctpop:
    case llvm::Intrinsic::bswap:
    case llvm::Intrinsic::ctlz:
    case llvm::Intrinsic::cttz:
    case llvm::Intrinsic::stacksave:
    case llvm::Intrinsic::stackrestore:
    case llvm::Intrinsic::get_dynamic_area_offset:
    case llvm::Intrinsic::returnaddress:
    case llvm::Intrinsic::frameaddress:
    case llvm::Intrinsic::addressofreturnaddress:
    case llvm::Intrinsic::prefetch:
    case llvm::Intrinsic::pcmarker:
    case llvm::Intrinsic::readcyclecounter:
    // case llvm::Intrinsic::dbg_declare:
    // case llvm::Intrinsic::dbg_label:
    case llvm::Intrinsic::eh_typeid_for:
    case llvm::Intrinsic::annotation:
    case llvm::Intrinsic::ptr_annotation:
    case llvm::Intrinsic::assume:
    case llvm::Intrinsic::experimental_noalias_scope_decl:
    case llvm::Intrinsic::var_annotation:
    case llvm::Intrinsic::memcpy:
    case llvm::Intrinsic::memmove:
    case llvm::Intrinsic::memset:
    case llvm::Intrinsic::sqrt:
    case llvm::Intrinsic::log:
    case llvm::Intrinsic::log2:
    case llvm::Intrinsic::log10:
    case llvm::Intrinsic::exp:
    case llvm::Intrinsic::exp2:
    case llvm::Intrinsic::pow:
    case llvm::Intrinsic::sin:
    case llvm::Intrinsic::cos:
    case llvm::Intrinsic::floor:
    case llvm::Intrinsic::ceil:
    case llvm::Intrinsic::trunc:
    case llvm::Intrinsic::round:
    case llvm::Intrinsic::roundeven:
    case llvm::Intrinsic::copysign:
    case llvm::Intrinsic::get_rounding:
    case llvm::Intrinsic::invariant_start:
    case llvm::Intrinsic::lifetime_start:
    case llvm::Intrinsic::invariant_end:
    case llvm::Intrinsic::lifetime_end:
        return true;
    default:
        return false;
    }
}

static void LowerIntrinsicCalls(llvm::Module &M){
    llvm::DataLayout DL(&M);
    std::unique_ptr<llvm::IntrinsicLowering> IL(new llvm::IntrinsicLowering(DL));
    for (llvm::Function &F : M) {
        for (llvm::BasicBlock &BB : F) {
            for (auto I = BB.begin(), E = BB.end(); I != E; ) {
                if (auto *Call = llvm::dyn_cast<llvm::CallInst>(&*I)) {
                    if (Call->getCalledFunction() && 
                        Call->getCalledFunction()->isIntrinsic()) {
                        llvm::Function *CalledFunc = Call->getCalledFunction();
                        llvm::Intrinsic::ID ID = (llvm::Intrinsic::ID)CalledFunc->getIntrinsicID();
                        
                        // Skip intrinsics that are not supported by IntrinsicLowering
                        // to avoid fatal errors
                        if (!isSupportedIntrinsic(ID)) {
                            ++I;
                            continue;
                        }
                        
                        // For supported intrinsics, use IntrinsicLowering
                        llvm::BasicBlock::iterator NextI = std::next(I);
                        IL->LowerIntrinsicCall(Call);
                        I = NextI;
                        continue;
                    }
                }
                ++I;
            }
        }
    }
}

// remove call @llvm.type.test and @llvm.assume
static void removeTypeTestAndAssume(llvm::Module &M){
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

std::unique_ptr<uppllvm::ExecutionEngine> initInterpreter() {
#ifdef DEBUG
    auto start_time = std::chrono::high_resolution_clock::now();
#endif

    if (_dtmc == "") {
        ERROR("Please set DTMC environment variable by export DTMC=/path/to/model-checking-flight-control");
        std::abort();
    }

    std::string IR_config_path = _dtmc + "/configs/IR_config.yml";
    _config = parseConfig(IR_config_path);

    std::string IR_path = _dtmc + "/" + _config.base.ir;

    // Create global LLVMContext that will outlive the Module
    GlobalContext = std::make_unique<llvm::LLVMContext>();
  
    // Load the bitcode...
    llvm::SMDiagnostic Err;
    std::unique_ptr<llvm::Module> Owner = parseIRFile(IR_path, Err, *GlobalContext);
    GlobalModule = Owner.get();
    if (!GlobalModule) {
        Err.print("initInterpreter", llvm::errs());
        exit(1);
    }
    // Remove llvm.type.test and llvm.assume calls
    removeTypeTestAndAssume(*GlobalModule);
    LowerIntrinsicCalls(*GlobalModule);
    // Stub out specified functions (replace body with simple ret)
    stubOutFunctions(*GlobalModule);
    // StripDebugInfo(*GlobalModule);
    // Load the whole bitcode file eagerly.
    {
        // Use *argv instead of argv[0] to work around a wrong GCC warning.
        llvm::ExitOnError ExitOnErr(IR_path +
                            ": bitcode didn't read correctly: ");
        ExitOnErr(GlobalModule->materializeAll());
    }

    // Load system libraries to make external functions available (like sysconf, malloc, etc.)
    // This is necessary for the interpreter to resolve external function symbols via libffi
    std::string LibErr;
    if (llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr, &LibErr)) {
        llvm::WithColor::warning(llvm::errs(), "initInterpreter") 
            << "Could not load program symbols: " << LibErr << "\n";
        ERROR("External function calls may fail.");
    }
    
    // Explicitly load C++ standard library to resolve C++ symbols when called from Python
    // This is needed because LoadLibraryPermanently(nullptr) may not work properly 
    // when the library is loaded as a shared object from Python via ctypes.
    // Try to load libstdc++ using dlopen with RTLD_GLOBAL to make symbols globally available.
    // This ensures C++ standard library symbols are accessible to the LLVM interpreter.
    bool cxxLibLoaded = false;
    
    // Try loading with RTLD_GLOBAL flag to make symbols globally available
    // First try common library names with version numbers
    std::vector<std::string> cxxLibs = {
        "libstdc++.so.6",    // GNU C++ standard library (most common on Linux)
        "libstdc++.so",      // Fallback without version
        "libc++.so.1",       // LLVM C++ standard library (alternative)
        "libc++.so"          // Fallback
    };
    
    for (const auto& libName : cxxLibs) {
        std::string cxxLibErr;
        if (!llvm::sys::DynamicLibrary::LoadLibraryPermanently(libName.c_str(), &cxxLibErr)) {
            cxxLibLoaded = true;
            llvm::WithColor::note(llvm::errs(), "initInterpreter")
                << "Successfully loaded C++ standard library: " << libName << "\n";
            break;
        }
    }
     
    if (!cxxLibLoaded) {
        llvm::WithColor::warning(llvm::errs(), "initInterpreter")
            << "Could not load C++ standard library. C++ symbols may not be resolvable.\n";
        ERROR("Could not load C++ standard library. C++ symbols may not be resolvable.");
        ERROR("When calling from Python, ensure libstdc++ is available in LD_LIBRARY_PATH.");
    }

    // Create the interpreter execution engine
    std::string ErrorMsg;
    std::unique_ptr<uppllvm::ExecutionEngine> EE(uppllvm::InstrsInterpreter::create(std::move(Owner), &ErrorMsg));
    if (!EE) {
        if (!ErrorMsg.empty())
            llvm::WithColor::error(llvm::errs())
            << "error creating EE: " << ErrorMsg << "\n";
        else
            llvm::WithColor::error(llvm::errs()) << "unknown error creating EE!\n";
        exit(1);
    }

    EE->runStaticConstructorsDestructors(false);

#ifdef DEBUG
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "[TIMING] initInterpreter: " << elapsed.count() << "s" << std::endl;
#endif

    return EE;
}

static std::unique_ptr<uppllvm::ExecutionEngine> EE_ptr = initInterpreter();

// Parse ELF file to get constant global variables and their address ranges
static void parseELFFile(const std::string& elfFile) {
#ifdef DEBUG
    auto start_time = std::chrono::high_resolution_clock::now();
#endif

    if (elfFile.empty()) {
        ERROR("ELF file is empty. Cannot parse ELF file.");
        abort();
    }

    if (!GlobalModule) {
        LLVM_ERROR("GlobalModule is not initialized. Cannot parse ELF file.");
        abort();
    }

    // Use LLVM's Object library to parse ELF file
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> bufferOrErr = 
        llvm::MemoryBuffer::getFile(elfFile);
    
    if (std::error_code ec = bufferOrErr.getError()) {
        ERROR("Failed to open ELF file: " + elfFile + " - " + ec.message());
        abort();
    }

    llvm::Expected<std::unique_ptr<llvm::object::ObjectFile>> objOrErr = 
        llvm::object::ObjectFile::createObjectFile(bufferOrErr.get()->getMemBufferRef());
    
    if (!objOrErr) {
        ERROR("Failed to parse ELF file: " + elfFile);
        llvm::consumeError(objOrErr.takeError());
        abort();
    }

    llvm::object::ObjectFile* objFile = objOrErr.get().get();
    
    // Check if it's an ELF file
    if (!objFile->isELF()) {
        ERROR("File is not an ELF file: " + elfFile);
        abort();
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
    
    // Clear previous results
    _constGVAddrRanges.clear();
    _longestStrConst = nullptr;
    
    // Match IR constant global variables with ELF symbols
    for (auto& GV : GlobalModule->globals()) {
        if (!GV.isConstant()) continue;
        constantGVCount++;
        
        std::string gvName = GV.getName().str();
        auto it = symbolMap.find(gvName);
        if (it != symbolMap.end()) {
            _constGVAddrRanges[&GV] = std::make_pair(it->second.first, it->second.second);
        } else {
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
                    }
                }
            }
        }
    }
    
    // Get strings section address range
    std::pair<uint64_t, uint64_t> stringsAddrRange = std::make_pair(0, 0);
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
            stringsAddrRange = std::make_pair(startAddr, endAddr);
            INFO("Found strings section at address range [" << startAddr
                      << ", " << endAddr << ")");
            break;
        }
    }

    if (_longestStrConst && stringsAddrRange.second > stringsAddrRange.first) {
        _constGVAddrRanges[_longestStrConst] = stringsAddrRange;
    }
    
    // for (const auto& entry : _constGVAddrRanges) {
    //     INFO("Constant global variable: " << entry.first->getName().str() 
    //          << " at address range [" << entry.second.first << ", " << entry.second.second << ")");
    // }
    
    INFO("=========================================");
    INFO("parsed ELF file: " + elfFile);
    INFO("Parsed " << _constGVAddrRanges.size() 
              << " constant global variables from ELF file.");
    INFO("Parsed " << stringCount
              << " string constants from ELF file.");
    INFO("Total symbols in ELF: " << symbolCount
              << ", global constants in IR: " << constantGVCount);
    INFO("=========================================");

#ifdef DEBUG
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "[TIMING] parseELFFile: " << elapsed.count() << "s" << std::endl;
#endif
}

static llvm::Type* getLLVMTypeFromString(const std::string& typeStr, llvm::LLVMContext& context) {
    if (typeStr == "i1") {
        return llvm::Type::getInt1Ty(context);
    } else if (typeStr == "i8") {
        return llvm::Type::getInt8Ty(context);
    } else if (typeStr == "i16") {
        return llvm::Type::getInt16Ty(context);
    } else if (typeStr == "i32") {
        return llvm::Type::getInt32Ty(context);
    } else if (typeStr == "i64") {
        return llvm::Type::getInt64Ty(context);
    } else if (typeStr == "float") {
        return llvm::Type::getFloatTy(context);
    } else if (typeStr == "double") {
        return llvm::Type::getDoubleTy(context);
    } else if (typeStr == "ptr") {
        return llvm::PointerType::getUnqual(context);
    } else {
        ERROR("Unsupported type string: " + typeStr);
        abort();
    }
}

static void findAllRegisterAndStackVarNeedsQuery(std::vector<std::string> &targetFuncsName) {
    if (!GlobalModule) {
        ERROR("GlobalModule is not initialized");
        abort();
    }

    _registerVarNeedsQuery.clear();
    _stackVarNeedsQuery.clear();

    std::map<std::string, std::vector<llvm::Instruction*>> localVarInstrsMap;

    for (const auto& funcName : targetFuncsName) {
        llvm::Function* F = GlobalModule->getFunction(funcName);
        if (!F) {
            ERROR("Function " + funcName + " not found in module.");
            abort();
        }

        for (auto& BB : *F) {
            for (auto& I : BB) {
                llvm::DILocation* debugLoc = I.getDebugLoc();
                std::string FullPath;
                int line = -1;
                int column = -1;
                if (debugLoc) {
                    line = debugLoc->getLine();
                    column = debugLoc->getColumn();
                    llvm::StringRef fileName = debugLoc->getFilename();
                    llvm::StringRef filePath = debugLoc->getDirectory();
                    if (filePath.empty()) {
                        filePath = ".";
                    }
                    FullPath = (llvm::Twine(filePath) + "/" + llvm::Twine(fileName)).str();

                    for (const auto& query : _config.registerQueries) {
                        bool fileMatch = (FullPath.find(query.file) != std::string::npos ||
                                          query.file.find(FullPath) != std::string::npos);

                        if (fileMatch && query.line == line && query.col == column) {
                            localVarInstrsMap[query.pname].push_back(&I);
                            _pVarTypes[query.pname] = getLLVMTypeFromString(query.type, GlobalModule->getContext());
                        }
                    }
                }

                // Process stack queries only for dbg.declare / dbg.addr intrinsics
                if (auto* DbgDeclare = llvm::dyn_cast<llvm::DbgDeclareInst>(&I)) {
                    llvm::DILocalVariable* DIVar = DbgDeclare->getVariable();
                    if (!DIVar) continue;

                    std::string localVarName = DIVar->getName().str();
                    if (!debugLoc) debugLoc = DbgDeclare->getDebugLoc();
                    if (!debugLoc) continue;

                    line = debugLoc->getLine();
                    llvm::StringRef fileName = debugLoc->getFilename();
                    llvm::StringRef filePath = debugLoc->getDirectory();
                    if (filePath.empty()) {
                        filePath = ".";
                    }
                    FullPath = (llvm::Twine(filePath) + "/" + llvm::Twine(fileName)).str();

                    for (const auto& query : _config.stackQueries) {
                        bool fileMatch = (FullPath.find(query.file) != std::string::npos ||
                                          query.file.find(FullPath) != std::string::npos);

                        if (fileMatch && query.line == line && query.localVarName == localVarName) {
                            llvm::Value* Address = DbgDeclare->getAddress();
                            if (auto* AllocaInst = llvm::dyn_cast<llvm::AllocaInst>(Address)) {
                                _stackVarNeedsQuery[query.pname] = std::make_pair(AllocaInst, query.offset);
                                // llvm::outs() << "Find AllocaInst: " << *AllocaInst << "\n";
                                _pVarTypes[query.pname] = getLLVMTypeFromString(query.type, GlobalModule->getContext());
                            }
                        }
                    }
                }
            }
        }
    }

    for (const auto& entry : localVarInstrsMap) {
        const std::string& varName = entry.first;
        const std::vector<llvm::Instruction*>& instrs = entry.second;
        llvm::Type* varType = _pVarTypes[varName];

        // Find all values with varType, if we find more than 1, we use the last one.
        llvm::Value* lastValue = nullptr;
        for (llvm::Instruction* instr : instrs) {
            if (instr->getType() == varType) {
                lastValue = instr;
            }
        }
        if (lastValue) {
            _registerVarNeedsQuery[varName] = lastValue;
        }
    }

    INFO("Found " << _registerVarNeedsQuery.size()
              << " register variables that need to be queried during interpretation.");
#ifdef DEBUG
    // Output register variable LLVM instructions in debug mode
    if (!_registerVarNeedsQuery.empty()) {
        llvm::outs() << "[DEBUG] Register variables:\n";
        for (const auto& entry : _registerVarNeedsQuery) {
            llvm::outs() << "  " << entry.first << ": " << *entry.second << "\n";
        }
    }
#endif 
    INFO("Found " << _stackVarNeedsQuery.size()
              << " stack variables that need to be queried during interpretation.");   
#ifdef DEBUG
    // Output stack variable LLVM instructions in debug mode
    if (!_stackVarNeedsQuery.empty()) {
        llvm::outs() << "[DEBUG] Stack variables:\n";
        for (const auto& entry : _stackVarNeedsQuery) {
            llvm::outs() << "  " << entry.first << ": " << *entry.second.first 
                         << " (offset: " << entry.second.second << ")\n";
        }
    }
#endif
}

/**
 * @brief Find and register all global variables that need to be queried during interpretation.
 * 
 * This function processes global variable queries from the IR configuration file and builds
 * a lookup table (_globalVarNeedsQuery) that maps property variable names to their corresponding
 * LLVM global variables and memory offsets.
 * 
 * The function also stores the type information for each property variable in _pVarTypes.
 * Type information is critical because:
 * 1. It determines how to correctly read data from memory (e.g., 4 bytes for i32, 8 bytes for double)
 * 2. It provides the bit-width needed to create properly-sized APInt values
 * 3. It enables type-specific operations in query functions (e.g., bit operations on integers)
 * 4. It guides the interpretation of raw memory bytes into meaningful values
 * 
 * @note This function should be called during initialization phase (initModel) after the IR module
 *       is loaded and before any property queries are performed.
 * @note The function reads configuration from _config.globalQueries which contains:
 *       - pname: Property variable name used in UPPAAL queries
 *       - globalVarName: Name of the global variable in LLVM IR
 *       - offset: Byte offset within the global variable (for struct members)
 *       - type: Type string ("i1", "i8", "i32", "float", "double", etc.)
 */
static void findAllGlobalVarNeedsQuery() {
    if (!GlobalModule) {
        ERROR("GlobalModule is not initialized");
        abort();
    }

    _globalVarNeedsQuery.clear();

    for (const auto& query : _config.globalQueries) {
        std::string globalVarName = query.globalVarName;
        llvm::GlobalVariable* GV = GlobalModule->getNamedGlobal(globalVarName);
        if (!GV) {
            ERROR("Global variable " + globalVarName + " not found in module.");
            abort();
        }
        uint64_t offset = query.offset;
        _globalVarNeedsQuery[query.pname] = std::make_pair(GV, offset);
        // Store type information - essential for correct memory interpretation and value extraction
        _pVarTypes[query.pname] = getLLVMTypeFromString(query.type, GlobalModule->getContext());
    }

    INFO("Found " << _globalVarNeedsQuery.size() 
              << " global variables that need to be queried during interpretation.");
#ifdef DEBUG
    // Output global variable LLVM instructions in debug mode
    if (!_globalVarNeedsQuery.empty()) {
        llvm::outs() << "[DEBUG] Global variables:\n";
        for (const auto& entry : _globalVarNeedsQuery) {
            llvm::outs() << "  " << entry.first << ": " << *entry.second.first 
                         << " (offset: " << entry.second.second << ")\n";
        }
    }
#endif
}

/**
 * @brief Retrieve the runtime value of a property variable during model interpretation.
 * 
 * This is the core function for property queries in the model checking system. It retrieves
 * the current value of a property variable from the interpreter's runtime state using a
 * three-tier lookup strategy: register variables -> stack variables -> global variables.
 * 
 * Type information plays a critical role throughout this function:
 * 
 * 1. **Memory Loading**: Types determine how many bytes to read and how to interpret them.
 *    - i8 reads 1 byte, i32 reads 4 bytes, double reads 8 bytes
 *    - The type guides LoadValueFromMemory to correctly deserialize raw memory into GenericValue
 * 
 * 2. **Default Value Creation**: When a variable hasn't been initialized yet, the type specifies
 *    how to create a zero-initialized value.
 *    - Integer types need the bit-width to create properly-sized APInt (e.g., APInt(32, 0))
 *    - Float/double types have different zero representations (0.0f vs 0.0)
 * 
 * 3. **Value Extraction**: Query functions (is_true, is_equal, etc.) use the type to interpret
 *    the GenericValue correctly.
 *    - i1 uses getBoolValue(), other integers use getSExtValue()
 *    - Floating-point types access FloatVal or DoubleVal fields
 * 
 * 4. **Bit-level Operations**: Functions like bit_is_true require integer types to perform
 *    bit indexing on the APInt representation.
 * 
 * @param pName The property variable name (defined in IR_config.yml)
 * @return GenericValue containing the runtime value with proper type interpretation
 * 
 * @note Search order priority:
 *       1. Register variables (SSA values in CFAContext)
 *       2. Stack variables (local variables with alloca instructions)
 *       3. Global variables (module-level variables)
 * @note The function aborts if the variable is not found in any of the three locations.
 * @note Type information is retrieved from _pVarTypes, which must be populated during initialization.
 */
static uppllvm::GenericValue getPropertyVarGV(const char* pName) {
    // First, try to get from register variables (SSA values in LLVM IR)
    auto regIt = _registerVarNeedsQuery.find(pName);
    if (regIt != _registerVarNeedsQuery.end()) {
        llvm::Value* var = regIt->second;
        uppllvm::InstrsInterpreter* interpreter = static_cast<uppllvm::InstrsInterpreter*>(EE_ptr.get());
        auto &localVals = interpreter->getCFAContext().Values;
        if (localVals.find(var) != localVals.end()) {
            return localVals[var];
        }else {
            // Variable not yet computed - create zero-initialized value based on type
            uppllvm::GenericValue gv;
            llvm::Type* varType = _pVarTypes[pName];
            if (varType->isIntegerTy()) {
                // Type determines the bit-width for APInt creation
                unsigned bitWidth = varType->getIntegerBitWidth();
                gv.IntVal = llvm::APInt(bitWidth, 0);
                return gv;
            } else if (varType->isFloatTy()) {
                gv.FloatVal = 0.0f;
                return gv;
            } else if (varType->isDoubleTy()) {
                gv.DoubleVal = 0.0;
                return gv;
            } else if (varType->isPointerTy()) {
                gv.PointerVal = nullptr;
                return gv;
            }else{
                LLVM_ERROR("Unsupported type: " << *varType);
                abort();
            }
        }
    }
    
    // Second, try to get from stack variables (local variables)
    auto stackIt = _stackVarNeedsQuery.find(pName);
    if (stackIt != _stackVarNeedsQuery.end()) {
        llvm::AllocaInst* allocaInst = stackIt->second.first;
        // llvm::outs() << "Getting stack variable for '" << pName << "': " << *allocaInst << "\n";
        uint64_t offset = stackIt->second.second;
        llvm::Type* varType = _pVarTypes[pName];  // Type needed for memory loading
        
        // Get the base address of the alloca instruction from the interpreter
        uppllvm::InstrsInterpreter* interpreter = static_cast<uppllvm::InstrsInterpreter*>(EE_ptr.get());
        auto &localVals = interpreter->getCFAContext().Values;
        
        // The alloca instruction should have an address stored in localVals
        auto allocaIt = localVals.find(allocaInst);
        if (allocaIt == localVals.end()) {
            // Stack allocation not found - return type-appropriate zero value
            uppllvm::GenericValue gv;
            llvm::Type* varType = _pVarTypes[pName];
            if (varType->isIntegerTy()) {
                unsigned bitWidth = varType->getIntegerBitWidth();
                gv.IntVal = llvm::APInt(bitWidth, 0);
                return gv;
            } else if (varType->isFloatTy()) {
                gv.FloatVal = 0.0f;
                return gv;
            } else if (varType->isDoubleTy()) {
                gv.DoubleVal = 0.0;
                return gv;
            } else if (varType->isPointerTy()) {
                gv.PointerVal = nullptr;
                return gv;
            }else{
                LLVM_ERROR("Unsupported type: " << *varType);
                abort();
            }
        }else {
            void* baseAddr = allocaIt->second.PointerVal;
            if (!baseAddr) {
                ERROR("Failed to get address for stack variable '" + std::string(pName) + "'");
                abort();
            }
            
            // Calculate final address and load value from memory
            // Type information tells LoadValueFromMemory how many bytes to read
            void* addr = (char*)baseAddr + offset;
            uppllvm::GenericValue gv;
            EE_ptr->LoadValueFromMemory(gv, (uppllvm::GenericValue*)addr, varType);
            return gv;
        }
    }
    
    // Third, try to get from global variables
    auto globalIt = _globalVarNeedsQuery.find(pName);
    if (globalIt != _globalVarNeedsQuery.end()) {
        // From target global variable, get the value based on the offset and type
        llvm::GlobalVariable* GV = globalIt->second.first;
        uint64_t offset = globalIt->second.second;
        llvm::Type* varType = _pVarTypes[pName];  // Type guides memory interpretation
        void* baseAddr = EE_ptr->getPointerToGlobalIfAvailable(GV);
        if (!baseAddr) {
            ERROR("Failed to get address for global variable '" + GV->getName().str() + "'");
            abort();
        }
        // Type determines how to deserialize the memory bytes into a GenericValue
        void* addr = (char*)baseAddr + offset;
        uppllvm::GenericValue gv;
        // llvm::outs() << "Loading global variable '" << pName 
        //                  << "' from address: " << addr << "\n";
        EE_ptr->LoadValueFromMemory(gv, (uppllvm::GenericValue*)addr, varType);
        return gv;
    }
    
    ERROR(std::string("Property variable not found: ") + pName);
    abort();
}

/**
 * @brief Convert property variable value to double.
 * 
 * This helper function extracts the double value from a property variable,
 * handling different LLVM types (integers, floats, doubles).
 * 
 * @param pName The property variable name
 * @return double The value converted to double
 */
static double getPropertyVarDoubleValue(const char* pName) {
    llvm::Type* pType = _pVarTypes[pName];
    if (!pType) {
        ERROR("Property variable type not found for: " << std::string(pName));
        abort();
    }

    uppllvm::GenericValue gv = getPropertyVarGV(pName);

    double varValue = 0.0;
    if (pType->isIntegerTy()) {
        varValue = static_cast<double>(gv.IntVal.getSExtValue());
    } else if (pType->isFloatTy()) {
        varValue = static_cast<double>(gv.FloatVal);
    } else if (pType->isDoubleTy()) {
        varValue = gv.DoubleVal;
    } else if (pType->isPointerTy()) {
        // Convert pointer address to double for comparison
        varValue = static_cast<double>(reinterpret_cast<uintptr_t>(gv.PointerVal));
    } else {
        LLVM_ERROR("Unsupported type for getPropertyVarDoubleValue: " << *pType);
        abort();
    }
    
    return varValue;
}

/**
 * @brief Evaluate an arithmetic expression containing property variables.
 * 
 * This function uses ExprTk to parse and evaluate expressions. All registered
 * property variables are made available to the expression, and ExprTk automatically
 * uses only the variables that appear in the expression.
 * 
 * @param expression The arithmetic expression string (e.g., "abs(var1 - var2)" or just "var1")
 * @return double The evaluated result
 */
static double evaluateExpression(const char* expression) {
    std::string expr_str(expression);
    
    // Set up exprtk
    typedef exprtk::symbol_table<double> symbol_table_t;
    typedef exprtk::expression<double> expression_t;
    typedef exprtk::parser<double> parser_t;
    
    symbol_table_t symbol_table;
    std::map<std::string, double> var_values;
    
    // Register all property variables in the symbol table
    // ExprTk will automatically use only the ones that appear in the expression
    for (const auto& entry : _pVarTypes) {
        const std::string& var_name = entry.first;
        double value = getPropertyVarDoubleValue(var_name.c_str());
        var_values[var_name] = value;
        symbol_table.add_variable(var_name, var_values[var_name]);
    }
    
    expression_t exprtk_expr;
    exprtk_expr.register_symbol_table(symbol_table);
    
    parser_t parser;
    if (!parser.compile(expr_str, exprtk_expr)) {
        ERROR("Failed to parse expression: " << expr_str);
        ERROR("Parser error: " << parser.error());
        abort();
    }
    
    double result = exprtk_expr.value();
    
    // Check for INF or NAN - warn but don't abort, let comparison functions handle it
    if (std::isinf(result) || std::isnan(result)) {
        llvm::outs() << "[Warning] Expression evaluation resulted in invalid value (INF or NAN): " 
                     << expr_str << " = " << result << "\n";
        // Don't abort here - let comparison functions return false for INF/NAN
    }
        
#ifdef DEBUG
    // Debug output: show expression and all variables used
    llvm::outs() << "[DEBUG] evaluateExpression: \"" << expr_str << "\" = " << result << "\n";
    llvm::outs() << "[DEBUG]   Variables used in expression:\n";
    for (const auto& var : var_values) {
        // Check if variable is actually used in the expression
        if (expr_str.find(var.first) != std::string::npos) {
            llvm::outs() << "[DEBUG]     " << var.first << " = " << var.second << "\n";
        }
    }
    llvm::outs() << "[DEBUG] --------------------------------\n";
#endif
    
    return result;
}

//=========================================================================================
// API For UPPAAL
//=========================================================================================

void printModule(){
    if (!GlobalModule) {
        ERROR("GlobalModule is not initialized");
        abort();
    }
    
    std::string property_name = _config.base.property_name;
    std::string target_dir = _dtmc + "/verifyDataBase/ir_and_elf/" + property_name + "/";
    
    // Create directory if it doesn't exist
    std::error_code EC;
    std::filesystem::create_directories(target_dir, EC);
    if (EC) {
        ERROR("Failed to create directory: " + target_dir + " - " + EC.message());
        abort();
    }
    
    // Get original IR file name from config
    std::string IR_path = _dtmc + "/" + _config.base.ir;
    std::filesystem::path original_path(IR_path);
    std::string filename = original_path.filename().string();
    
    // Change extension to .ll if needed
    if (filename.find(".bc") != std::string::npos) {
        filename.replace(filename.find(".bc"), 3, ".ll");
    } else if (filename.find(".ll") == std::string::npos) {
        // If no extension or different extension, add .ll
        filename += ".ll";
    }
    
    // Determine output file path
    std::string output_path = target_dir + filename;
    
    // Write module to file as LLVM IR text format
    std::error_code FileErr;
    llvm::raw_fd_ostream OS(output_path, FileErr, llvm::sys::fs::OF_None);
    if (FileErr) {
        ERROR("Failed to open file for writing: " + output_path + " - " + FileErr.message());
        abort();
    }
    
    // Write module as LLVM IR text format (.ll)
    GlobalModule->print(OS, nullptr);
    OS.flush();
    
    if (OS.has_error()) {
        ERROR("Failed to write IR to file: " + output_path);
        abort();
    }
    INFO("Module printed to file: " + output_path);
}

void buildCFAModels(){
#ifdef DEBUG
    auto start_time = std::chrono::high_resolution_clock::now();
#endif

    std::string property = _config.base.property_name;
    std::string targetFuncsLogFile = _dtmc + "/verifyDataBase/func_slice/" + property + "_Slice_FS.yml";
    // Parse LLVM-PDG's slice.
    std::vector<std::string> targetFuncsName = targetFuncsConfig(targetFuncsLogFile);
    std::string entryPointFuncName = _config.base.entrypoints[0];
    std::size_t call_depth = _config.base.call_depth;
    
    findAllRegisterAndStackVarNeedsQuery(targetFuncsName);

    // Use factory to create CFAModels
#ifdef DEBUG
    if (!CFAModelFactory::createCFAModels(GlobalModule, targetFuncsName, entryPointFuncName, 
        call_depth, _CFAModels, _entryPointCFAModel, true)) {
        ERROR("Failed to build CFAModels");
        abort();
    }
    INFO("Debug mode: CFAModel built successfully.");
    // INFO("CFAModel built successfully. Saving to dot file...");
    // _entryPointCFAModel->ToDot("cfa_model.dot");
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "[TIMING] buildCFAModels: " << elapsed.count() << "s" << std::endl;
#else    
    if (!CFAModelFactory::createCFAModels(GlobalModule, targetFuncsName, entryPointFuncName, 
                                        call_depth, _CFAModels, _entryPointCFAModel, true)) {
        ERROR("Failed to build CFAModels");
        abort();
    }
#endif
}

int nbOfLocations () {
    return _entryPointCFAModel->getLocations().size();
}

int nbOfOutgoingEdges (int loc) {
    return _entryPointCFAModel->getLocation(loc)->getOutgoingEdges().size();
}

int getSuccLocID (int loc, int e) {
    return _entryPointCFAModel->getLocation(loc)->getOutgoingEdges()[e]->getDstLoc()->getLocId();
}

int getInitLocationID () {
    return _entryPointCFAModel->getEntryLocation()->getLocId();
}

const char* getNameOfLocation (int loc) {
    uppllvm::CFALocation* location = _entryPointCFAModel->getLocation(loc);
    int id = location->getLocId();
    std::string temp = "L" + std::to_string(id);
    if (location->getBB()) {
        std::string bbNum = location->getBB()->getName().str();
        temp += " + " + bbNum;
    }
    char * ret = new char[temp.size() + 1];
    strcpy(ret, temp.c_str());
    return ret;
}

bool hasGuard(int l, int e) {
    uppllvm::CFAEdge* edge = _entryPointCFAModel->getLocation(l)->getOutgoingEdges()[e];
    return edge->hasCondBrVar();
}

int evaluateGuard(int l, int e) {
    uppllvm::CFAEdge* edge = _entryPointCFAModel->getLocation(l)->getOutgoingEdges()[e];
    if (!hasGuard(l, e)) {
        ERROR("Edge " + std::to_string(e) + " in location " + std::to_string(l) + " does not have a conditional branch variable");
        abort();
    }
    llvm::Value* condBrVar = edge->getCondBrVar();
    
    // Get the value from CFAContext, check if it exists
    uppllvm::InstrsInterpreter* interpreter = static_cast<uppllvm::InstrsInterpreter*>(EE_ptr.get());
    auto& cfaContext = interpreter->getCFAContext();
    auto it = cfaContext.Values.find(condBrVar);
    if (it == cfaContext.Values.end()) {
        LLVM_ERROR("Conditional branch variable " << *condBrVar << " not found in CFAContext");
        abort();
    }
    
    // Extract boolean value from IntVal (booleans in LLVM are typically i1, stored in IntVal)
    uppllvm::GenericValue& gv = it->second;
    bool guard = gv.IntVal.getBoolValue();
    // llvm::outs() << "guard: " << gv.IntVal << "\n";
    // llvm::outs() << "guard: " << guard << "\n";
    return guard ? 1 : 0;
}

int getEdgeType(int l, int e) {
    uppllvm::CFAEdge* edge = _entryPointCFAModel->getLocation(l)->getOutgoingEdges()[e];
    uppllvm::EdgeType type = edge->getType();
    return static_cast<int>(type);
}

void executeEdge (int loc, int e) {
    uppllvm::CFAEdge* edge = _entryPointCFAModel->getLocation(loc)->getOutgoingEdges()[e];
    const std::vector<uppllvm::CFAInstruction>& instructions = edge->getInstructions();
    uppllvm::InstrsInterpreter* interpreter = static_cast<uppllvm::InstrsInterpreter*>(EE_ptr.get());
    std::vector<uppllvm::CFAInstruction> instrsCopy(instructions.begin(), instructions.end());
#ifdef DEBUG
    llvm::outs() << "[DEBUG] Executing edge from location " << loc << " via edge " << e << "\n";
#endif
    interpreter->runInstrs(instrsCopy);
 }

int getInstructionCount (int loc, int e) {
    uppllvm::CFAEdge* edge = _entryPointCFAModel->getLocation(loc)->getOutgoingEdges()[e];
    return edge->getInstructions().size();
}

bool is_true (const char* pName1) {
    double result = evaluateExpression(pName1);
    // Return false if result is INF or NAN
    if (std::isinf(result) || std::isnan(result)) {
        return false;
    }
    return result != 0.0;
}

bool is_false (const char* pName1) {
    return !is_true(pName1);
}

bool bit_is_true (const char* pName1, int bitIndex){
    double result = evaluateExpression(pName1);
    // Return false if result is INF or NAN
    if (std::isinf(result) || std::isnan(result)) {
        return false;
    }
    int64_t intResult = static_cast<int64_t>(result);
    return (intResult >> bitIndex) & 1;
}

bool bit_is_false (const char* pName1, int bitIndex){
    return !bit_is_true(pName1, bitIndex);
}

bool is_equal_const (const char* pName1, double constVal) {
    double result = evaluateExpression(pName1);
    // Return false if result is INF or NAN (INF/NAN comparisons are always false)
    if (std::isinf(result) || std::isnan(result)) {
        return false;
    }
    return result == constVal;
}

bool is_equal (const char* pName1, const char* pName2) {
    double constVal = evaluateExpression(pName2);
    // Return false if constVal is INF or NAN
    if (std::isinf(constVal) || std::isnan(constVal)) {
        return false;
    }
    return is_equal_const(pName1, constVal);
}

bool is_not_equal_const (const char* pName1, double constVal) {
    double result = evaluateExpression(pName1);
    // Return false if result is INF or NAN
    if (std::isinf(result) || std::isnan(result)) {
        return false;
    }
    return result != constVal;
}

bool is_not_equal (const char* pName1, const char* pName2) {
    double result1 = evaluateExpression(pName1);
    double result2 = evaluateExpression(pName2);
    // Return false if either result is INF or NAN
    if (std::isinf(result1) || std::isnan(result1) || 
        std::isinf(result2) || std::isnan(result2)) {
        return false;
    }
    return result1 != result2;
}

bool is_greater_const (const char* pName1, double constVal) {
    double result = evaluateExpression(pName1);
    // Return false if result is INF or NAN
    if (std::isinf(result) || std::isnan(result)) {
        return false;
    }
    return result > constVal;
}

bool is_greater (const char* pName1, const char* pName2) {
    double constVal = evaluateExpression(pName2);
    // Return false if constVal is INF or NAN
    if (std::isinf(constVal) || std::isnan(constVal)) {
        return false;
    }
    return is_greater_const(pName1, constVal);
}

bool is_less_const (const char* pName1, double constVal) {
    double result = evaluateExpression(pName1);
    // Return false if result is INF or NAN
    if (std::isinf(result) || std::isnan(result)) {
        return false;
    }
    return result < constVal;
}

bool is_less (const char* pName1, const char* pName2) {
    double constVal = evaluateExpression(pName2);
    // Return false if constVal is INF or NAN
    if (std::isinf(constVal) || std::isnan(constVal)) {
        return false;
    }
    return is_less_const(pName1, constVal);
}

bool is_greater_or_equal_const (const char* pName1, double constVal) {
    double result = evaluateExpression(pName1);
    // Return false if result is INF or NAN
    if (std::isinf(result) || std::isnan(result)) {
        return false;
    }
    return result >= constVal;
}

bool is_greater_or_equal (const char* pName1, const char* pName2) {
    double result1 = evaluateExpression(pName1);
    double result2 = evaluateExpression(pName2);
    // Return false if either result is INF or NAN
    if (std::isinf(result1) || std::isnan(result1) || 
        std::isinf(result2) || std::isnan(result2)) {
        return false;
    }
    return result1 >= result2;
}

bool is_less_or_equal_const (const char* pName1, double constVal) {
    double result = evaluateExpression(pName1);
    // Return false if result is INF or NAN
    if (std::isinf(result) || std::isnan(result)) {
        return false;
    }
    return result <= constVal;
}

bool is_less_or_equal (const char* pName1, const char* pName2) {
    double result1 = evaluateExpression(pName1);
    double result2 = evaluateExpression(pName2);
    // Return false if either result is INF or NAN
    if (std::isinf(result1) || std::isnan(result1) || 
        std::isinf(result2) || std::isnan(result2)) {
        return false;
    }
    return result1 <= result2;
}

/**
 * @brief Parse params_config.yml and create global variables for params_config pointer references.
 * 
 * This function reads the params_config.yml file and creates LLVM global variables in the module
 * for each parameter configuration. This allows the variables to be found by name lookup and
 * queried like regular global variables.
 * 
 * Format of params_config.yml:
 *   PropertyName:
 *     params:
 *       - id: <param_id>
 *         type: <type_string>  (e.g., "i64", "struct.FailsafeBase::State")
 *         value: <value>       (single value or array for aggregate types)
 */
static void loadParamsConfig() {
    if (!GlobalModule) {
        ERROR("GlobalModule is not initialized. Cannot load params_config.");
        abort();
    }
    
    std::string configPath = _dtmc + "/InterpreterR/base/params_config.yml";
    
    if (!std::filesystem::exists(configPath)) {
        INFO("No params_config.yml found at: " << configPath);
        return;
    }
    
    try {
        YAML::Node config = YAML::LoadFile(configPath);
        std::string property_name = _config.base.property_name;
        
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
        llvm::LLVMContext& Context = *GlobalContext;
        
        for (const auto& param : params) {
            int id = param["id"].as<int>();
            std::string typeStr = param["type"].as<std::string>();
            std::string paramGVName = "params_config_" + std::to_string(id);
            std::string paramKey = property_name + "_param_" + std::to_string(id);
            
            llvm::GlobalVariable* paramGV = nullptr;
            size_t dataSize = 0;
            
            // Check if it's an aggregate type (struct, class, or array)
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
                    *GlobalModule,
                    arrayType,
                    true,  // constant
                    llvm::GlobalValue::InternalLinkage,
                    initializer,
                    paramGVName
                );
                
                INFO("Created params_config global variable: " << paramGVName << " (aggregate, " << dataSize << " bytes)");
                
                // Allocate memory and copy data
                void* memPtr = malloc(dataSize);
                if (!memPtr) {
                    ERROR("Failed to allocate memory for params_config parameter");
                    abort();
                }
                std::memcpy(memPtr, bytes.data(), dataSize);
                
                // Map the allocated memory to the global variable in ExecutionEngine
                EE_ptr->addGlobalMapping(paramGV, memPtr);
                _paramsConfigData[paramKey] = {memPtr, dataSize};
            } else {
                // Scalar type
                llvm::Type* llvmType = getLLVMTypeFromString(typeStr, Context);
                if (!llvmType) {
                    ERROR("Failed to get LLVM type for: " << typeStr);
                    abort();
                }
                
                llvm::Constant* initValue = nullptr;
                if (llvmType->isIntegerTy()) {
                    uint64_t value = param["value"].as<uint64_t>();
                    initValue = llvm::ConstantInt::get(llvmType, value);
                    dataSize = llvmType->getIntegerBitWidth() / 8;
                } else if (llvmType->isFloatTy()) {
                    float value = param["value"].as<float>();
                    initValue = llvm::ConstantFP::get(llvmType, value);
                    dataSize = 4;
                } else if (llvmType->isDoubleTy()) {
                    double value = param["value"].as<double>();
                    initValue = llvm::ConstantFP::get(llvmType, value);
                    dataSize = 8;
                } else {
                    ERROR("Unsupported scalar type: " << typeStr);
                    abort();
                }
                
                paramGV = new llvm::GlobalVariable(
                    *GlobalModule,
                    llvmType,
                    true,  // constant
                    llvm::GlobalValue::InternalLinkage,
                    initValue,
                    paramGVName
                );
                
                INFO("Created params_config global variable: " << paramGVName << " (scalar, " << dataSize << " bytes)");
                
                // Allocate memory and store value
                void* memPtr = malloc(dataSize);
                if (!memPtr) {
                    ERROR("Failed to allocate memory for params_config parameter");
                    abort();
                }
                
                if (llvmType->isIntegerTy()) {
                    uint64_t value = param["value"].as<uint64_t>();
                    std::memcpy(memPtr, &value, dataSize);
                } else if (llvmType->isFloatTy()) {
                    float value = param["value"].as<float>();
                    std::memcpy(memPtr, &value, dataSize);
                } else if (llvmType->isDoubleTy()) {
                    double value = param["value"].as<double>();
                    std::memcpy(memPtr, &value, dataSize);
                }
                
                // Map the allocated memory to the global variable in ExecutionEngine
                EE_ptr->addGlobalMapping(paramGV, memPtr);
                _paramsConfigData[paramKey] = {memPtr, dataSize};
            }
        }
    } catch (const YAML::Exception& e) {
        ERROR("Failed to parse params_config.yml: " << e.what());
        abort();
    }
}

void initMemory (const char* modelInputPath){
    // parse model input protobuf file
    std::ifstream input(modelInputPath);
    if (!input.is_open()) {
        ERROR("Failed to open file " + std::string(modelInputPath));
        abort();
    }
    google::protobuf::io::IstreamInputStream zero_copy_stream(&input);
    google::protobuf::io::CodedInputStream coded_stream(&zero_copy_stream);
    modelInputs::ModelInputs modelInputs;
    modelInputs.ParseFromCodedStream(&coded_stream);
    input.close();

#ifdef DEBUG
    // Store a copy for debug output
    DebugModelInputs.CopyFrom(modelInputs);
#endif

    // Use global Module and Context directly
    llvm::Module* Mod = GlobalModule;
    llvm::LLVMContext& Context = *GlobalContext;

    // Derive ELF file path from IR file path
    std::string property_name = _config.base.property_name;
    std::string elfFile;
    // Determine prefix based on IR file path
    std::string IR_path = _dtmc + "/" + _config.base.ir;
    std::string prefix;
    std::string lowerIR = IR_path;
    std::transform(lowerIR.begin(), lowerIR.end(), lowerIR.begin(), ::tolower);
    if (lowerIR.find("px4") != std::string::npos) {
        prefix = "px4";
    } else if (lowerIR.find("arducopter") != std::string::npos) {
        prefix = "arducopter";
    } else {
        ERROR("Unknown IR file: " + IR_path);
        abort();
    }
    
    // Load union override configuration
    loadUnionOverrideConfig(prefix);
    
    // Construct ELF file path
    std::string elfDir = _dtmc + "/verifyDataBase/ir_and_elf/" + property_name + "/";
    if (!prefix.empty()) {
        elfFile = elfDir + prefix + "_" + property_name;
    } else {
        // Fallback: look for any file in the same directory (assuming first file is the ELF)
        if (std::filesystem::exists(elfDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(elfDir)) {
                if (entry.is_regular_file()) {
                    elfFile = entry.path().string();
                    break;
                }
            }
        }
    }
    // Parse ELF file if available
    if (!elfFile.empty()) {
        parseELFFile(elfFile);
    }

    struct logicAddrRange {
        uint64_t start;
        uint64_t end;
        std::string name;
    };
    std::vector<logicAddrRange> logicAddrRanges;
    size_t heapStart = 0;
    for (const auto &globalVar : modelInputs.globalvars()) {
        logicAddrRange range;
        range.start = globalVar.offset();
        range.end = range.start + globalVar.size();
        heapStart += globalVar.size();
        range.name = globalVar.name();
        logicAddrRanges.push_back(range);
    }
    
    size_t heap_size = 0; 
    for (const auto &heapVar : modelInputs.heapvars()) {
        heap_size += heapVar.size();
    }
    void* heapBaseAddr = nullptr;
    if (heap_size > 0) {
        // create global variable @heap
        llvm::Type* i8Type = llvm::Type::getInt8Ty(Context);
        llvm::ArrayType* heapArrayType = llvm::ArrayType::get(i8Type, heap_size);
        llvm::Constant* zeroInitializer = llvm::ConstantAggregateZero::get(heapArrayType);
        llvm::GlobalVariable* heapGlobal = new llvm::GlobalVariable(
            *Mod,                                    // Module
            heapArrayType,                           // Type
            false,                                   // isConstant
            llvm::GlobalValue::ExternalLinkage,      // Linkage
            zeroInitializer,                         // Initializer
            "heap"                                   // Name
        );
        // Allocate memory for @heap and add to GlobalAddressMap
        EE_ptr->getMemoryAndMapGV(heapGlobal);
        // Get heap base address
        heapBaseAddr = EE_ptr->getPointerToGlobalIfAvailable(heapGlobal);
        if (!heapBaseAddr) {
            ERROR("Failed to allocate memory for heap");
            abort();
        }
        logicAddrRange range;
        range.start = heapStart;
        range.end = range.start + heap_size;
        range.name = "heap";
        logicAddrRanges.push_back(range);
    }
    
    // Add constant global variables' address ranges from ELF file
    for (const auto& entry : _constGVAddrRanges) {
        llvm::GlobalVariable* gv = entry.first;
        if (!gv) continue;
        uint64_t startAddr = entry.second.first;
        uint64_t endAddr = entry.second.second;
        if (startAddr < endAddr) {
            logicAddrRange range;
            range.start = startAddr;
            range.end = endAddr;
            range.name = gv->getName().str();
            logicAddrRanges.push_back(range);
        }
    }
    
    // Initialize global variables' values
    for (const auto &globalVar : modelInputs.globalvars()) {
        // Use getNamedGlobal to include internal linkage variables
        llvm::GlobalVariable* GV = Mod->getNamedGlobal(globalVar.name());
        
        if (!GV) {
            llvm::errs() << "[WARNING] Global variable '" << globalVar.name() 
                        << "' not found in module. Skipping...\n";
            continue;
        }
        
        void* Addr = EE_ptr->getPointerToGlobalIfAvailable(GV);
        if (!Addr) {
            llvm::errs() << "[WARNING] Failed to get address for global variable '" 
                        << globalVar.name() << "'. Skipping...\n";
            continue;
        }

        // Create GenericValue and fill data
        uppllvm::GenericValue GVal;
        llvm::Type* GVType = GV->getValueType();
        if (globalVar.type() == modelInputs::TypeSpec::INT) {
            unsigned BitWidth = GVType->getIntegerBitWidth();
            GVal.IntVal = llvm::APInt(BitWidth, globalVar.i64_value());
            EE_ptr->StoreValueToMemory(GVal, (uppllvm::GenericValue*)Addr, GVType);
        } else if (globalVar.type() == modelInputs::TypeSpec::FLOAT) {
            GVal.FloatVal = globalVar.f_value();
            EE_ptr->StoreValueToMemory(GVal, (uppllvm::GenericValue*)Addr, GVType);
        } else if (globalVar.type() == modelInputs::TypeSpec::DOUBLE) {
            GVal.DoubleVal = globalVar.d_value();
            EE_ptr->StoreValueToMemory(GVal, (uppllvm::GenericValue*)Addr, GVType);
        } else if (globalVar.type() == modelInputs::TypeSpec::PTR) {
            // Convert logical offset to physical address
            int64_t ptrOffset = globalVar.i64_value();
            for (const auto& range : logicAddrRanges) {
                if (ptrOffset >= range.start && ptrOffset < range.end) {
                    GVal.PointerVal = (char*)EE_ptr->getPointerToGlobalIfAvailable(
                        Mod->getNamedGlobal(range.name)) + (ptrOffset - range.start);
                    break;
                }
            }
            EE_ptr->StoreValueToMemory(GVal, (uppllvm::GenericValue*)Addr, GVType);
        } else if (globalVar.type() == modelInputs::TypeSpec::AGGR) {
            if (globalVar.has_members()) {
                // For aggregate types, process each member
                for (const auto& member : globalVar.members().members()) {
                    uppllvm::GenericValue MemberVal;
                    void* MemberAddr = (char*)Addr + member.member_offset();
                    
                    llvm::Type* MemberType = nullptr;
                    if (member.type() == modelInputs::TypeSpec::INT) {
                        unsigned BitWidth = member.size() * 8; // 64-bit
                        MemberVal.IntVal = llvm::APInt(BitWidth, member.i64_value());
                        MemberType = llvm::Type::getIntNTy(Context, BitWidth);
                    } else if (member.type() == modelInputs::TypeSpec::FLOAT) {
                        MemberVal.FloatVal = member.f_value();
                        MemberType = llvm::Type::getFloatTy(Context);
                    } else if (member.type() == modelInputs::TypeSpec::DOUBLE) {
                        MemberVal.DoubleVal = member.d_value();
                        MemberType = llvm::Type::getDoubleTy(Context);
                    } else if (member.type() == modelInputs::TypeSpec::PTR) {
                        // Convert logical offset to physical address for member pointers
                        int64_t ptrOffset = member.i64_value();
                        for (const auto& range : logicAddrRanges) {
                            if (ptrOffset >= range.start && ptrOffset < range.end) {
                                MemberVal.PointerVal = (char*)EE_ptr->getPointerToGlobalIfAvailable(
                                    Mod->getNamedGlobal(range.name)) + (ptrOffset - range.start);
                                break;
                            }
                        }
                        MemberType = llvm::Type::getInt8PtrTy(Context);
                    }
                    if (MemberType) {
                        EE_ptr->StoreValueToMemory(MemberVal, (uppllvm::GenericValue*)MemberAddr, MemberType);
                    }
                }
            }
        }
    }
    
    // Initialize heap variables' values
    if (heapBaseAddr != nullptr) {
        for (const auto &heapVar : modelInputs.heapvars()) {
            void* varAddr = (char*)heapBaseAddr + heapVar.offset() - heapStart;
            
            if (heapVar.type() == modelInputs::TypeSpec::INT) {
                uppllvm::GenericValue Val;
                unsigned BitWidth = heapVar.size() * 8;
                Val.IntVal = llvm::APInt(BitWidth, heapVar.i64_value());
                llvm::Type* VarType = llvm::Type::getIntNTy(Context, BitWidth);
                EE_ptr->StoreValueToMemory(Val, (uppllvm::GenericValue*)varAddr, VarType);
            } else if (heapVar.type() == modelInputs::TypeSpec::FLOAT) {
                uppllvm::GenericValue Val;
                Val.FloatVal = heapVar.f_value();
                EE_ptr->StoreValueToMemory(Val, (uppllvm::GenericValue*)varAddr, 
                                          llvm::Type::getFloatTy(Context));
            } else if (heapVar.type() == modelInputs::TypeSpec::DOUBLE) {
                uppllvm::GenericValue Val;
                Val.DoubleVal = heapVar.d_value();
                EE_ptr->StoreValueToMemory(Val, (uppllvm::GenericValue*)varAddr, 
                                          llvm::Type::getDoubleTy(Context));
            } else if (heapVar.type() == modelInputs::TypeSpec::PTR) {
                // Convert logical offset to physical address
                uppllvm::GenericValue Val;
                int64_t ptrOffset = heapVar.i64_value();
                for (const auto& range : logicAddrRanges) {
                    if (ptrOffset >= range.start && ptrOffset < range.end) {
                        Val.PointerVal = (char*)EE_ptr->getPointerToGlobalIfAvailable(
                            Mod->getNamedGlobal(range.name)) + (ptrOffset - range.start);
                        break;
                    }
                }
                EE_ptr->StoreValueToMemory(Val, (uppllvm::GenericValue*)varAddr, 
                                          llvm::Type::getInt8PtrTy(Context));
            } else if (heapVar.type() == modelInputs::TypeSpec::AGGR && heapVar.has_members()) {
                // Check if this heapVar has a union override
                uint64_t heapVarHash = heapVar.hash();
                const HeapVarUnionOverride* unionOverride = nullptr;
                for (const auto& override : _heapVarUnionOverrides) {
                    if (override.hash == heapVarHash) {
                        unionOverride = &override;
                        INFO("\t- Applying union override for heap variable with hash " << heapVarHash);
                        break;
                    }
                }
                
                // Prepare union buffer if union override exists
                std::vector<uint8_t> unionBuffer;
                uint64_t unionStartOffset = -1;
                uint64_t unionEndOffset = -1;
                
                // Map to store type information for each offset in unionBuffer
                // Key: offset in unionBuffer, Value: LLVM type at that offset
                std::map<uint64_t, llvm::Type*> unionBufferTypeMap;
                
                if (unionOverride) {
                    // Get the real struct type based on union override configuration
                    llvm::StructType* realStructType = getRealStructType(unionOverride, property_name, Context);
                    
                    if (realStructType) {
                        unionStartOffset = unionOverride->startOffset;
                        unionEndOffset = unionOverride->endOffset;
                        uint64_t unionSize = unionEndOffset - unionStartOffset;
                        
                        // Step 1: Create temporary buffer for union region and fill with MI data (without pointer conversion)
                        unionBuffer.resize(unionSize, 0);
                        
                        for (const auto& member : heapVar.members().members()) {
                            uint64_t memberOffset = member.member_offset();
                            // Only process members within the union region
                            if (memberOffset >= unionStartOffset && memberOffset < unionEndOffset) {
                                uint64_t bufferOffset = memberOffset - unionStartOffset;
                                if (member.type() == modelInputs::TypeSpec::INT) {
                                    int64_t value = member.i64_value();
                                    unsigned size = member.size();
                                    if (bufferOffset + size <= unionSize) {
                                        memcpy(&unionBuffer[bufferOffset], &value, size);
                                    }
                                } else if (member.type() == modelInputs::TypeSpec::FLOAT) {
                                    float value = member.f_value();
                                    if (bufferOffset + sizeof(value) <= unionSize) {
                                        memcpy(&unionBuffer[bufferOffset], &value, sizeof(value));
                                    }
                                } else if (member.type() == modelInputs::TypeSpec::DOUBLE) {
                                    double value = member.d_value();
                                    if (bufferOffset + sizeof(value) <= unionSize) {
                                        memcpy(&unionBuffer[bufferOffset], &value, sizeof(value));
                                    }
                                } else if (member.type() == modelInputs::TypeSpec::PTR) {
                                    // Store logical address first (will be converted later)
                                    int64_t logicalAddr = member.i64_value();
                                    if (bufferOffset + sizeof(logicalAddr) <= unionSize) {
                                        memcpy(&unionBuffer[bufferOffset], &logicalAddr, sizeof(logicalAddr));
                                    }
                                }
                            }
                        }
                        
                        // Step 2: Use real struct type to remap pointers in the union buffer
                        llvm::DataLayout DL(Mod);

                        // Lambda function to recursively process types and remap pointers
                        std::function<void(llvm::Type*, uint64_t)> processTypeRecursive = 
                            [&](llvm::Type* type, uint64_t offsetInBuffer) {
                            if (!type) return;
                            
                            if (type->isPointerTy()) {
                                // Record pointer type information
                                unionBufferTypeMap[offsetInBuffer] = type;
                                
                                // Process pointer type - remap logical to physical address
                                if (offsetInBuffer < unionSize && offsetInBuffer + sizeof(void*) <= unionSize) {
                                    // Read logical address from buffer
                                    int64_t logicalAddr = 0;
                                    memcpy(&logicalAddr, &unionBuffer[offsetInBuffer], sizeof(logicalAddr));
                                    
                                    // Convert logical address to physical address
                                    void* physicalAddr = nullptr;
                                    for (const auto& range : logicAddrRanges) {
                                        if (logicalAddr >= (int64_t)range.start && logicalAddr < (int64_t)range.end) {
                                            physicalAddr = (char*)EE_ptr->getPointerToGlobalIfAvailable(
                                                Mod->getNamedGlobal(range.name)) + (logicalAddr - range.start);
                                            break;
                                        }
                                    }
                                    
                                    // Write physical address back to buffer
                                    memcpy(&unionBuffer[offsetInBuffer], &physicalAddr, sizeof(physicalAddr));
                                }
                            } else if (llvm::StructType* structType = llvm::dyn_cast<llvm::StructType>(type)) {
                                // Recursively process nested struct members
                                const llvm::StructLayout* structLayout = DL.getStructLayout(structType);
                                
                                for (unsigned i = 0; i < structType->getNumElements(); ++i) {
                                    llvm::Type* memberType = structType->getElementType(i);
                                    uint64_t memberOffset = structLayout->getElementOffset(i);
                                    uint64_t absoluteOffset = offsetInBuffer + memberOffset;
                                    
                                    // Recursively process this member
                                    processTypeRecursive(memberType, absoluteOffset);
                                }
                            } else if (llvm::ArrayType* arrayType = llvm::dyn_cast<llvm::ArrayType>(type)) {
                                // Process array elements
                                llvm::Type* elementType = arrayType->getElementType();
                                uint64_t elementSize = DL.getTypeAllocSize(elementType);
                                uint64_t numElements = arrayType->getNumElements();
                                
                                for (uint64_t i = 0; i < numElements; ++i) {
                                    uint64_t elementOffset = offsetInBuffer + (i * elementSize);
                                    
                                    // Recursively process this array element
                                    processTypeRecursive(elementType, elementOffset);
                                }
                            } else {
                                // For primitive types (int, float, double, etc.), record type information
                                unionBufferTypeMap[offsetInBuffer] = type;
                            }
                        };
                        
                        // Start recursive processing from the root struct type at offset 0
                        processTypeRecursive(realStructType, 0);
                    }
                }
                
                // Step 3: Write back data from unionBuffer to heap variable memory
                for (const auto& entry : unionBufferTypeMap) {
                    uint64_t offset = entry.first;
                    llvm::Type* type = entry.second;
                    
                    void* targetAddr = (char*)varAddr + unionStartOffset + offset;
                    uppllvm::GenericValue gv;
                    
                    if (type->isIntegerTy()) {
                        unsigned bitWidth = type->getIntegerBitWidth();
                        unsigned byteSize = (bitWidth + 7) / 8;
                        if (offset + byteSize <= unionBuffer.size()) {
                            uint64_t value = 0;
                            memcpy(&value, &unionBuffer[offset], byteSize);
                            gv.IntVal = llvm::APInt(bitWidth, value);
                            EE_ptr->StoreValueToMemory(gv, (uppllvm::GenericValue*)targetAddr, type);
                            // llvm::outs() << "[xxxxxx]   Writing back integer of bitWidth " << bitWidth 
                            //              << " at offset " << offset << " with value " << gv.IntVal << "\n";
                        }
                    } else if (type->isFloatTy()) {
                        if (offset + sizeof(float) <= unionBuffer.size()) {
                            memcpy(&gv.FloatVal, &unionBuffer[offset], sizeof(float));
                            EE_ptr->StoreValueToMemory(gv, (uppllvm::GenericValue*)targetAddr, type);
                            // llvm::outs() << "[xxxxxx]   Writing back float at offset " << offset 
                            //              << " with value " << gv.FloatVal << "\n";
                        }
                    } else if (type->isDoubleTy()) {
                        if (offset + sizeof(double) <= unionBuffer.size()) {
                            memcpy(&gv.DoubleVal, &unionBuffer[offset], sizeof(double));
                            EE_ptr->StoreValueToMemory(gv, (uppllvm::GenericValue*)targetAddr, type);
                            // llvm::outs() << "[xxxxxx]   Writing back double at offset " << offset 
                            //              << " with value " << gv.DoubleVal << "\n";
                        }
                    } else if (type->isPointerTy()) {
                        if (offset + sizeof(void*) <= unionBuffer.size()) {
                            memcpy(&gv.PointerVal, &unionBuffer[offset], sizeof(void*));
                            EE_ptr->StoreValueToMemory(gv, (uppllvm::GenericValue*)targetAddr, type);
                            // llvm::outs() << "[xxxxxx]   Writing back pointer at offset " << offset 
                            //              << " with value " << gv.PointerVal << "\n";
                        }
                    }else {
                        ERROR("Unsupported type when writing back to heap variable");
                        abort();
                    }
                }

                for (const auto& member : heapVar.members().members()) {
                    uint64_t memberOffset = member.member_offset();
                    if (unionOverride != nullptr && memberOffset >= unionStartOffset && memberOffset < unionEndOffset){
                        continue; // skip members within union region
                    }
                    // Read data from MI (original approach)
                    uppllvm::GenericValue MemberVal;
                    void* MemberAddr = (char*)varAddr + member.member_offset();

                    llvm::Type* MemberType = nullptr;
                    if (member.type() == modelInputs::TypeSpec::INT) {
                        unsigned BitWidth = member.size() * 8;
                        MemberVal.IntVal = llvm::APInt(BitWidth, member.i64_value());
                        MemberType = llvm::Type::getIntNTy(Context, BitWidth);
                    } else if (member.type() == modelInputs::TypeSpec::FLOAT) {
                        MemberVal.FloatVal = member.f_value();
                        MemberType = llvm::Type::getFloatTy(Context);
                    } else if (member.type() == modelInputs::TypeSpec::DOUBLE) {
                        MemberVal.DoubleVal = member.d_value();
                        MemberType = llvm::Type::getDoubleTy(Context);
                    } else if (member.type() == modelInputs::TypeSpec::PTR) {
                        // Convert logical offset to physical address for member pointers
                        int64_t ptrOffset = member.i64_value();
                        for (const auto& range : logicAddrRanges) {
                            if (ptrOffset >= range.start && ptrOffset < range.end) {
                                MemberVal.PointerVal = (char*)EE_ptr->getPointerToGlobalIfAvailable(
                                    Mod->getNamedGlobal(range.name)) + (ptrOffset - range.start);
                                break;
                            }
                        }
                        MemberType = llvm::Type::getInt8PtrTy(Context);
                    }
                    if (MemberType) {
                        EE_ptr->StoreValueToMemory(MemberVal, (uppllvm::GenericValue*)MemberAddr, MemberType);
                    }
                }
            }
        }
    }
}

void initParams() {
    _entryArgs.clear();
    for (const auto& param : _config.paramList) {
        uppllvm::GenericValue gv;
        if (param.second.first == "i1"){
            gv.IntVal = llvm::APInt(1, std::stoi(param.second.second));
            _entryArgs.push_back(gv);
        }
        else if (param.second.first == "i8"){
            gv.IntVal = llvm::APInt(8, std::stoi(param.second.second));
            _entryArgs.push_back(gv);
        }
        else if (param.second.first == "i16"){
            gv.IntVal = llvm::APInt(16, std::stoi(param.second.second));
            _entryArgs.push_back(gv);
        }
        else if (param.second.first == "i32"){
            gv.IntVal = llvm::APInt(32, std::stoi(param.second.second));
            _entryArgs.push_back(gv);
        }
        else if (param.second.first == "i64"){
            gv.IntVal = llvm::APInt(64, std::stoll(param.second.second));
            _entryArgs.push_back(gv);
        }
        else if (param.second.first == "float"){
            gv.FloatVal = std::stof(param.second.second);
            _entryArgs.push_back(gv);
        }
        else if (param.second.first == "double"){
            gv.DoubleVal = std::stod(param.second.second);
            _entryArgs.push_back(gv);
        }
        else if (param.second.first == "ptr"){
            std::string ptrStr = param.second.second;
            // check ptrStr's format is "name + offset"
            size_t plusPos = ptrStr.find(" + ");
            if (plusPos == std::string::npos) {
                ERROR("Invalid pointer format: " + ptrStr);
                abort();
            }
            std::string varName = ptrStr.substr(0, plusPos);
            uint32_t offset = std::stoul(ptrStr.substr(plusPos + 3));
            
            // Check if this is a params_config reference
            if (varName == "params_config") {
                std::string property_name = _config.base.property_name;
                std::string paramKey = property_name + "_param_" + std::to_string(offset);
                
                auto it = _paramsConfigData.find(paramKey);
                if (it == _paramsConfigData.end()) {
                    ERROR("params_config parameter not found: " + paramKey + " (offset=" + std::to_string(offset) + ")");
                    abort();
                }
                
                gv.PointerVal = it->second.first;
                _entryArgs.push_back(gv);
                INFO("Set parameter to params_config pointer: " << paramKey << " -> " << gv.PointerVal);
            } else {
                // Regular global variable reference
                llvm::GlobalVariable* GV = GlobalModule->getNamedGlobal(varName);
                if (!GV) {
                    ERROR("Global variable not found: " + varName);
                    abort();
                }
                void* baseAddr = EE_ptr->getPointerToGlobalIfAvailable(GV);
                if (!baseAddr) {
                    ERROR("Failed to get address of global variable: " + varName);
                    abort();
                }
                gv.PointerVal = (char*)baseAddr + offset;
                _entryArgs.push_back(gv);
            }
        }
        else{
            ERROR("Unsupported parameter type: " + param.second.first);
            abort();
        }
    }
    
    // Set parameters to entry point function using SetValue
    if (!_entryArgs.empty()) {
        std::string entryPointFuncName = _config.base.entrypoints[0];
        llvm::Function* entryFunc = GlobalModule->getFunction(entryPointFuncName);
        if (entryFunc) {
            uppllvm::InstrsInterpreter* interpreter = 
                static_cast<uppllvm::InstrsInterpreter*>(EE_ptr.get());
            interpreter->setEntryPointParams(entryFunc, _entryArgs);
        } else {
            ERROR("Entry point function '" + entryPointFuncName + "' not found. Parameters not set.");
            abort();
        }
    }
}

void initModel () {
#ifdef DEBUG
    auto start_time = std::chrono::high_resolution_clock::now();
#endif

    std::string modelInputPath = utils::getEnv("MODEL_INPUT_PATH");
    if (modelInputPath.empty()) {
        ERROR("Environment variable MODEL_INPUT_PATH is not set.");
        abort();
    }
    INFO("Initializing model with input file: " + modelInputPath);
    loadParamsConfig();
    initMemory(modelInputPath.c_str());
    initParams();
    findAllGlobalVarNeedsQuery();

#ifdef DEBUG
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "[TIMING] initModel: " << elapsed.count() << "s" << std::endl;
#endif
}

#ifdef DEBUG
void showMemory() {
    llvm::Module* Mod = GlobalModule;
    llvm::LLVMContext& Context = *GlobalContext;
    
    std::cout << "\n========== Memory State After initMemory ==========\n\n";
    
    // Show all global variables from protobuf
    std::cout << "=== Global Variables (" << DebugModelInputs.globalvars_size() << " variables) ===\n\n";
    
    for (const auto &globalVar : DebugModelInputs.globalvars()) {
        llvm::GlobalVariable* GV = Mod->getNamedGlobal(globalVar.name());
        void* Addr = GV ? EE_ptr->getPointerToGlobalIfAvailable(GV) : nullptr;
        
        std::cout << "Variable: " << globalVar.name() << "\n";
        std::cout << "  Offset: " << globalVar.offset() << "\n";
        std::cout << "  Size: " << globalVar.size() << " bytes\n";
        std::cout << "  Address: " << globalVar.addr() << " -> " << Addr << "\n";
        std::cout << "  Type: ";
        
        switch (globalVar.type()) {
            case modelInputs::TypeSpec::INT:
                std::cout << "INT (i" << (globalVar.size() * 8) << ")\n";
                std::cout << "  Value: " << globalVar.i64_value();
                if (Addr) {
                    // Read actual value from memory
                    if (globalVar.size() == 1) {
                        std::cout << " [Memory: " << (int)*(int8_t*)Addr << "]";
                    } else if (globalVar.size() == 2) {
                        std::cout << " [Memory: " << *(int16_t*)Addr << "]";
                    } else if (globalVar.size() == 4) {
                        std::cout << " [Memory: " << *(int32_t*)Addr << "]";
                    } else if (globalVar.size() == 8) {
                        std::cout << " [Memory: " << *(int64_t*)Addr << "]";
                    }
                }
                std::cout << "\n";
                break;
                
            case modelInputs::TypeSpec::FLOAT:
                std::cout << "FLOAT\n";
                std::cout << "  Value: " << globalVar.f_value();
                if (Addr) {
                    std::cout << " [Memory: " << *(float*)Addr << "]";
                }
                std::cout << "\n";
                break;
                
            case modelInputs::TypeSpec::DOUBLE:
                std::cout << "DOUBLE\n";
                std::cout << "  Value: " << globalVar.d_value();
                if (Addr) {
                    std::cout << " [Memory: " << *(double*)Addr << "]";
                }
                std::cout << "\n";
                break;
                
            case modelInputs::TypeSpec::PTR:
                std::cout << "PTR\n";
                std::cout << "  Value (logical offset): " << globalVar.i64_value();
                if (Addr) {
                    std::cout << " [Memory: " << *(void**)Addr << "]";
                }
                std::cout << "\n";
                break;
                
            case modelInputs::TypeSpec::AGGR:
                std::cout << "AGGREGATE (struct/array)\n";
                if (globalVar.has_members()) {
                    std::cout << "  Members (" << globalVar.members().members_size() << "):\n";
                    for (const auto& member : globalVar.members().members()) {
                        std::cout << "    Offset " << member.member_offset() << ": ";
                        switch (member.type()) {
                            case modelInputs::TypeSpec::INT:
                                std::cout << "i" << (member.size() * 8) << " = " << member.i64_value();
                                if (Addr) {
                                    void* memberAddr = (char*)Addr + member.member_offset();
                                    if (member.size() == 1) {
                                        std::cout << " [Memory: " << (int)*(int8_t*)memberAddr << "]";
                                    } else if (member.size() == 2) {
                                        std::cout << " [Memory: " << *(int16_t*)memberAddr << "]";
                                    } else if (member.size() == 4) {
                                        std::cout << " [Memory: " << *(int32_t*)memberAddr << "]";
                                    } else if (member.size() == 8) {
                                        std::cout << " [Memory: " << *(int64_t*)memberAddr << "]";
                                    }
                                }
                                break;
                            case modelInputs::TypeSpec::FLOAT:
                                std::cout << "float = " << member.f_value();
                                if (Addr) {
                                    void* memberAddr = (char*)Addr + member.member_offset();
                                    std::cout << " [Memory: " << *(float*)memberAddr << "]";
                                }
                                break;
                            case modelInputs::TypeSpec::DOUBLE:
                                std::cout << "double = " << member.d_value();
                                if (Addr) {
                                    void* memberAddr = (char*)Addr + member.member_offset();
                                    std::cout << " [Memory: " << *(double*)memberAddr << "]";
                                }
                                break;
                            case modelInputs::TypeSpec::PTR:
                                std::cout << "ptr = " << member.i64_value();
                                if (Addr) {
                                    void* memberAddr = (char*)Addr + member.member_offset();
                                    std::cout << " [Memory: " << *(void**)memberAddr << "]";
                                }
                                break;
                            default:
                                std::cout << "unknown type";
                        }
                        std::cout << "\n";
                    }
                }
                break;
                
            default:
                std::cout << "UNKNOWN\n";
        }
        std::cout << "\n";
    }
    
    // Show heap contents
    if (DebugModelInputs.heapvars_size() > 0) {
        std::cout << "=== Heap Variables (" << DebugModelInputs.heapvars_size() << " variables) ===\n\n";
        
        llvm::GlobalVariable* heapGV = Mod->getNamedGlobal("heap");
        void* heapBaseAddr = heapGV ? EE_ptr->getPointerToGlobalIfAvailable(heapGV) : nullptr;
        
        // Calculate heap start offset (sum of all global var sizes)
        size_t heapStart = 0;
        for (const auto &globalVar : DebugModelInputs.globalvars()) {
            heapStart += globalVar.size();
        }
        
        for (const auto &heapVar : DebugModelInputs.heapvars()) {
            void* varAddr = heapBaseAddr ? (char*)heapBaseAddr + heapVar.offset() - heapStart : nullptr;
            
            std::cout << "Heap Var Hash: 0x" << std::hex << heapVar.hash() << std::dec << "\n";
            std::cout << "  Offset: " << heapVar.offset() << "\n";
            std::cout << "  Size: " << heapVar.size() << " bytes\n";
            std::cout << "  Address: " << heapVar.addr() << " -> " << varAddr << "\n";
            std::cout << "  Type: ";
            
            switch (heapVar.type()) {
                case modelInputs::TypeSpec::INT:
                    std::cout << "INT (i" << (heapVar.size() * 8) << ")\n";
                    std::cout << "  Value: " << heapVar.i64_value();
                    if (varAddr) {
                        if (heapVar.size() == 1) {
                            std::cout << " [Memory: " << (int)*(int8_t*)varAddr << "]";
                        } else if (heapVar.size() == 2) {
                            std::cout << " [Memory: " << *(int16_t*)varAddr << "]";
                        } else if (heapVar.size() == 4) {
                            std::cout << " [Memory: " << *(int32_t*)varAddr << "]";
                        } else if (heapVar.size() == 8) {
                            std::cout << " [Memory: " << *(int64_t*)varAddr << "]";
                        }
                    }
                    std::cout << "\n";
                    break;
                    
                case modelInputs::TypeSpec::FLOAT:
                    std::cout << "FLOAT\n";
                    std::cout << "  Value: " << heapVar.f_value();
                    if (varAddr) {
                        std::cout << " [Memory: " << *(float*)varAddr << "]";
                    }
                    std::cout << "\n";
                    break;
                    
                case modelInputs::TypeSpec::DOUBLE:
                    std::cout << "DOUBLE\n";
                    std::cout << "  Value: " << heapVar.d_value();
                    if (varAddr) {
                        std::cout << " [Memory: " << *(double*)varAddr << "]";
                    }
                    std::cout << "\n";
                    break;
                    
                case modelInputs::TypeSpec::PTR:
                    std::cout << "PTR\n";
                    std::cout << "  Value (logical offset): " << heapVar.i64_value();
                    if (varAddr) {
                        std::cout << " [Memory: " << *(void**)varAddr << "]";
                    }
                    std::cout << "\n";
                    break;
                    
                case modelInputs::TypeSpec::AGGR:
                    std::cout << "AGGREGATE (struct/array)\n";
                    if (heapVar.has_members()) {
                        std::cout << "  Members (" << heapVar.members().members_size() << "):\n";
                        for (const auto& member : heapVar.members().members()) {
                            std::cout << "    Offset " << member.member_offset() << ": ";
                            switch (member.type()) {
                                case modelInputs::TypeSpec::INT:
                                    std::cout << "i" << (member.size() * 8) << " = " << member.i64_value();
                                    if (varAddr) {
                                        void* memberAddr = (char*)varAddr + member.member_offset();
                                        if (member.size() == 1) {
                                            std::cout << " [Memory: " << (int)*(int8_t*)memberAddr << "]";
                                        } else if (member.size() == 2) {
                                            std::cout << " [Memory: " << *(int16_t*)memberAddr << "]";
                                        } else if (member.size() == 4) {
                                            std::cout << " [Memory: " << *(int32_t*)memberAddr << "]";
                                        } else if (member.size() == 8) {
                                            std::cout << " [Memory: " << *(int64_t*)memberAddr << "]";
                                        }
                                    }
                                    break;
                                case modelInputs::TypeSpec::FLOAT:
                                    std::cout << "float = " << member.f_value();
                                    if (varAddr) {
                                        void* memberAddr = (char*)varAddr + member.member_offset();
                                        std::cout << " [Memory: " << *(float*)memberAddr << "]";
                                    }
                                    break;
                                case modelInputs::TypeSpec::DOUBLE:
                                    std::cout << "double = " << member.d_value();
                                    if (varAddr) {
                                        void* memberAddr = (char*)varAddr + member.member_offset();
                                        std::cout << " [Memory: " << *(double*)memberAddr << "]";
                                    }
                                    break;
                                case modelInputs::TypeSpec::PTR:
                                    std::cout << "ptr = " << member.i64_value();
                                    if (varAddr) {
                                        void* memberAddr = (char*)varAddr + member.member_offset();
                                        std::cout << " [Memory: " << *(void**)memberAddr << "]";
                                    }
                                    break;
                                default:
                                    std::cout << "unknown type";
                            }
                            std::cout << "\n";
                        }
                    }
                    break;
                    
                default:
                    std::cout << "UNKNOWN\n";
            }
            std::cout << "\n";
        }
    }
    
    std::cout << "===================================================\n\n";
}

void showEntryArgs(){
    std::cout << "\n=== Entry Function Arguments (" << _entryArgs.size() << " arguments) ===\n\n";
    for (size_t i = 0; i < _entryArgs.size(); ++i) {
        const auto& arg = _entryArgs[i];
        std::cout << "Argument " << i << ":\n";
        
        // Always show all possible type interpretations
        
        // Integer interpretation
        if (arg.IntVal.getBitWidth() > 0) {
            llvm::SmallString<40> str;
            arg.IntVal.toString(str, 10, true, false);
            std::cout << "  INT (i" << arg.IntVal.getBitWidth() << ") = " << str.c_str() << "\n";
        } else {
            std::cout << "  INT = (no value, bitwidth = 0)\n";
        }
        
        // Float interpretation
        std::cout << "  FLOAT = " << arg.FloatVal << "\n";
        
        // Double interpretation
        std::cout << "  DOUBLE = " << arg.DoubleVal << "\n";
        
        // Pointer interpretation
        std::cout << "  PTR = " << arg.PointerVal << "\n";
        
        std::cout << "\n";
    }
    std::cout << "=========================================\n\n";
}

void showQueries() {
    for (auto & pair : _pVarTypes) {
        const std::string& pName = pair.first;
        llvm::Type* varType = pair.second;
        uppllvm::GenericValue gv = getPropertyVarGV(pName.c_str());
        if (varType->isIntegerTy()) {
            llvm::outs() << pName << " = " << gv.IntVal << "\n";
        } else if (varType->isFloatTy()) {
            llvm::outs() << pName << " = " << gv.FloatVal << "\n";
        } else if (varType->isDoubleTy()) {
            llvm::outs() << pName << " = " << gv.DoubleVal << "\n";
        } else if (varType->isPointerTy()) {
            llvm::outs() << pName << " = " << gv.PointerVal << "\n";
        } else {
            LLVM_ERROR("Unsupported type: " << *varType);
            abort();
        }
    }
    std::cout << "=========================================\n\n";
}

#else
void showMemory() {
    // No-op in non-debug builds
}

void showEntryArgs(){
    // No-op in non-debug builds
}
#endif
