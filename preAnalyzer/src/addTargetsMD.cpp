#include <yaml-cpp/yaml.h>
#include "user_utils.hh"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/TypedPointerType.h"
#include "llvm/Support/Casting.h"
#include <llvm/CodeGen/StableHashing.h> 

#include <boost/algorithm/string.hpp>

#include <map>
#include <set>
#include <string>
#include <sstream>
#include <fstream>

using namespace llvm;

struct inCallTargets {
    bool isVCall;
    std::set<std::string> targets;
};

// Used for handing DMATypeAdd YAML file
struct CallSiteInfo {
    std::string DMAType;
    bool isBaseType;
    CallSiteInfo(std::string DMAType, bool isBaseType) : DMAType(DMAType), isBaseType(isBaseType) {}
};

struct DMATypeAddItem {
    std::string _caller_name = "";
    std::string _callee_name = "";
    std::map<uint32_t, CallSiteInfo> _callSiteInfoMap;
    DMATypeAddItem() {}
    DMATypeAddItem(std::string caller_name) : _caller_name(caller_name) {}
    DMATypeAddItem(std::string caller_name, std::string callee_name) : _caller_name(caller_name), _callee_name(callee_name) {}
};

struct DMATypeAdd {
    std::vector<DMATypeAddItem> _simpleDMATypeAdd;
    std::vector<std::vector<std::string>> _encapsulatedDMAFuncs;
    std::vector<DMATypeAddItem> _complexDMATypeAdd;
};

std::map<int, inCallTargets> TypeMDInCallEdges;
std::map<int, inCallTargets> SVFInCallEdges;
std::map<int, inCallTargets> DeepTypeInCallEdges;
std::map<int, inCallTargets> manualInCallEdges; // manual identified indirect calls
std::map<int, inCallTargets> incallEdges;
std::map<int, llvm::CallBase*> incallID2CallBase;
std::unique_ptr<DMATypeAdd> _DMAType = nullptr;

bool writeYAMLFile(const std::string &outputFilePath, const std::map<int, inCallTargets> &incallEdgesMap) {
    std::ofstream outFile(outputFilePath);
    if (!outFile.is_open()) {
        ERROR("Failed to open output file: " << outputFilePath);
        return false;
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "indirectCalls";
    out << YAML::Value << YAML::BeginSeq;

    for (const auto &pair : incallEdgesMap) {
        out << YAML::BeginMap;
        out << YAML::Key << "inCallID" << YAML::Value << pair.first;
        out << YAML::Key << "isvcall" << YAML::Value << pair.second.isVCall;
        out << YAML::Key << "targets" << YAML::Value << YAML::BeginSeq;
        for (const auto &target : pair.second.targets) {
            out << target;
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }

    out << YAML::EndSeq;
    out << YAML::EndMap;

    outFile << out.c_str();
    outFile.close();
    
    return true;
}

void parseYAML(const std::string &Filename, std::map<int, inCallTargets> &incallEdgesMap) {
    YAML::Node config = YAML::LoadFile(Filename);
    for (auto &&node : config["indirectCalls"]) {
        int ID = node["inCallID"].as<int>();
        bool isVirtual = node["isvcall"].as<bool>();
        
        inCallTargets callTargets;
        callTargets.isVCall = isVirtual;
        
        for (auto &&target : node["targets"]) {
            callTargets.targets.insert(target.as<std::string>());
        }
        
        incallEdgesMap[ID] = callTargets;
    }
}

DMATypeAdd parseDMATypeYAML(std::string DMATypeAddYamlPath) {
    std::ifstream fin(DMATypeAddYamlPath);
    if (!fin.is_open()) {
        ERROR("Can't open file: " << DMATypeAddYamlPath);
        exit(1);
    }
    DMATypeAdd dmaType;
    YAML::Node root = YAML::Load(fin);
  
    if (const auto& root_mode = root["DMATypeAdd"]["DMAType"]) {
        if (const auto& SimpleCase = root_mode["SimpleCase"]) {
            for (const auto& caller_item : SimpleCase) {
                DMATypeAddItem item(caller_item["Caller"].as<std::string>());
                if (caller_item["callsites"]) {
                    for (const auto& callsite : caller_item["callsites"]) {
                        item._callSiteInfoMap.emplace(callsite["ID"].as<uint32_t>(), 
                            CallSiteInfo(callsite["DMAType"].as<std::string>(), 
                                       callsite["isBaseType"].as<bool>()));
                    }
                }
                dmaType._simpleDMATypeAdd.push_back(item);
            }
        }
        
        if (const auto& ComplexCase = root_mode["ComplexCase"]) {
            for (const auto& encapsulatedDMAFunc : ComplexCase["EncapsulatedDMAFuncs"]) {
                std::vector<std::string> call_chain;
                if (encapsulatedDMAFunc.IsScalar()) {
                    std::string call_chain_str = encapsulatedDMAFunc.as<std::string>();
                    std::vector<std::string> call_chain_vec;
                    boost::split(call_chain_vec, call_chain_str, boost::is_any_of(" - "));
                    for (auto& tmp : call_chain_vec) {
                        if (tmp != "") {
                            call_chain.push_back(boost::trim_copy(tmp));
                        }
                    }
                }
                if (!call_chain.empty()) {
                    dmaType._encapsulatedDMAFuncs.push_back(call_chain);
                }
            }
            for (const auto& caller_item : ComplexCase["InnerSimpleCase"]) {
                DMATypeAddItem item(
                    caller_item["Caller"].as<std::string>(), 
                    caller_item["Callee"].as<std::string>());
                if (caller_item["callsites"]) {
                    for (const auto& callsite : caller_item["callsites"]) {
                        item._callSiteInfoMap.emplace(callsite["ID"].as<uint32_t>(), 
                            CallSiteInfo(callsite["DMAType"].as<std::string>(), 
                                        callsite["isBaseType"].as<bool>()));
                    }
                }
                dmaType._complexDMATypeAdd.push_back(item);
            }
        } else {
            WARNING("Can't find ComplexCase in " << "DMAType");
            // exit(1);
        }
    } else {
        ERROR("Can't find " << "DMAType" << " in " << DMATypeAddYamlPath);
        exit(1);
    }
    
    return dmaType;
}

llvm::StringRef getStructTypeNamePrefix(llvm::StringRef Name) {
    while (!Name.empty()) {
        size_t DotPos = Name.rfind('.');
        if (DotPos == llvm::StringRef::npos || DotPos == 0 || DotPos == Name.size() - 1)
            return Name;
            
        // Check if the part after the dot is all digits
        bool AllDigits = true;
        for (size_t i = DotPos + 1; i < Name.size(); ++i) {
            if (!isdigit(static_cast<unsigned char>(Name[i]))) {
                AllDigits = false;
                break;
            }
        }
        if (!AllDigits)
            return Name;
            
        Name = Name.substr(0, DotPos);
    }
    return Name;
}

size_t hashType(llvm::Type* Ty) {
    switch (Ty->getTypeID()) {
        case llvm::Type::VoidTyID:      return llvm::stable_hash_combine_string("void");
        case llvm::Type::HalfTyID:      return llvm::stable_hash_combine_string("half");
        case llvm::Type::BFloatTyID:    return llvm::stable_hash_combine_string("bfloat");
        case llvm::Type::FloatTyID:     return llvm::stable_hash_combine_string("float");
        case llvm::Type::DoubleTyID:    return llvm::stable_hash_combine_string("double");
        case llvm::Type::X86_FP80TyID:  return llvm::stable_hash_combine_string("x86_fp80");
        case llvm::Type::FP128TyID:     return llvm::stable_hash_combine_string("fp128");
        case llvm::Type::PPC_FP128TyID: return llvm::stable_hash_combine_string("ppc_fp128");
        case llvm::Type::LabelTyID:     return llvm::stable_hash_combine_string("label");
        case llvm::Type::MetadataTyID:  return llvm::stable_hash_combine_string("metadata");
        case llvm::Type::X86_MMXTyID:   return llvm::stable_hash_combine_string("x86_mmx");
        case llvm::Type::X86_AMXTyID:   return llvm::stable_hash_combine_string("x86_amx");
        case llvm::Type::TokenTyID:     return llvm::stable_hash_combine_string("token");
        case llvm::Type::IntegerTyID: {
            std::string intType = "i" + std::to_string(llvm::cast<llvm::IntegerType>(Ty)->getBitWidth());
            return llvm::stable_hash_combine_string(intType);
        }
        case llvm::Type::PointerTyID:   return llvm::stable_hash_combine_string("ptr");
        case llvm::Type::FunctionTyID: {
            llvm::FunctionType *FTy = llvm::cast<llvm::FunctionType>(Ty);
            size_t retHash = hashType(FTy->getReturnType());
            for (llvm::Type *paramTy : FTy->params()) {
                retHash = llvm::stable_hash_combine(retHash, hashType(paramTy));
            }
            llvm::StringRef varArg = FTy->isVarArg() ? "varArg:true" : "varArg:false";
            return llvm::stable_hash_combine(retHash, llvm::stable_hash_combine_string(varArg));
        }
        case llvm::Type::StructTyID: {
            llvm::StructType *STy = llvm::cast<llvm::StructType>(Ty);
        
            if (!STy->isLiteral()) {
                size_t nameHash = llvm::stable_hash_combine_string(getStructTypeNamePrefix(STy->getName()));
                llvm::StringRef packed = STy->isPacked() ? "packed:true" : "packed:false";
                size_t retHash = llvm::stable_hash_combine_string(packed);
                for (llvm::Type *elemTy : STy->elements()) {
                    nameHash = llvm::stable_hash_combine(nameHash, hashType(elemTy));
                }
                return llvm::stable_hash_combine(nameHash, retHash);
            } else {
                llvm::StringRef packed = STy->isPacked() ? "packed:true" : "packed:false";
                size_t retHash = llvm::stable_hash_combine_string(packed);
                for (llvm::Type *elemTy : STy->elements()) {
                    retHash = llvm::stable_hash_combine(retHash, hashType(elemTy));
                }
                return retHash;
            }
        }
        case llvm::Type::ArrayTyID: {
            llvm::ArrayType *ATy = llvm::cast<llvm::ArrayType>(Ty);
            size_t elemHash = hashType(ATy->getElementType());
            std::string STStr;
            llvm::raw_string_ostream STStream(STStr);
            STStream << ATy->getNumElements();
            return llvm::stable_hash_combine(elemHash, llvm::stable_hash_combine_string(STStream.str()));
        }
        case llvm::Type::FixedVectorTyID:
        case llvm::Type::ScalableVectorTyID: {
            llvm::VectorType *PTy = llvm::cast<llvm::VectorType>(Ty);
            llvm::ElementCount EC = PTy->getElementCount();
            size_t elemHash = hashType(PTy->getElementType());
            if (EC.isScalable()) {
                return llvm::stable_hash_combine(elemHash, llvm::stable_hash_combine_string("vscale"));
            } else {
                std::string STStr;
                llvm::raw_string_ostream STStream(STStr);
                STStream << EC.getKnownMinValue();
                return llvm::stable_hash_combine(elemHash, llvm::stable_hash_combine_string(STStream.str()));
            }
        }
        case llvm::Type::TypedPointerTyID: {
            llvm::TypedPointerType *TPTy = llvm::cast<llvm::TypedPointerType>(Ty);
            size_t elemHash = hashType(TPTy->getElementType());
            return llvm::stable_hash_combine(llvm::stable_hash_combine_string("typedptr"), elemHash);
        }
        case llvm::Type::TargetExtTyID: {
            llvm::TargetExtType *TETy = llvm::cast<llvm::TargetExtType>(Ty);
            std::string STStr;
            llvm::raw_string_ostream STStream(STStr);
            STStream << "target(\"";
            llvm::printEscapedString(Ty->getTargetExtName(), STStream);
            STStream << "\"";
            for (llvm::Type *Inner : TETy->type_params())
                STStream << ", " << *Inner;
            for (unsigned IntParam : TETy->int_params())
                STStream << ", " << IntParam;
            STStream << ")";
            return llvm::stable_hash_combine_string(STStream.str());
        }
    }
    llvm_unreachable("Invalid TypeID");
}

uint64_t computeStructHash(llvm::Module &M, const std::string& typeName) {
    for (auto& type : M.getIdentifiedStructTypes()) {
        if (getStructTypeNamePrefix(type->getName()) == typeName) {
            // 使用更复杂的哈希计算方式，考虑结构体的实际内容
            return hashType(type);
        }
    }
    WARNING("Cannot find struct type: " << typeName << ", using string hash");
    return llvm::stable_hash_combine_string(typeName);
}

void applyTypeHashMetadata(llvm::CallBase* CI, const std::string& typeName, bool isBaseType, uint64_t typeHash) {
    llvm::LLVMContext &ctx = CI->getContext();
    
    llvm::Metadata* typeNameMD = llvm::MDString::get(ctx, typeName);
    llvm::Metadata* isBaseTypeMD = llvm::ConstantAsMetadata::get(
        llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), isBaseType));
    llvm::Metadata* typeHashMD = llvm::ConstantAsMetadata::get(
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), typeHash));
    
    llvm::MDNode* heapAllocTypeAddMD = llvm::MDNode::get(ctx, {typeNameMD, isBaseTypeMD, typeHashMD});
    CI->setMetadata("heapAllocTypeAdd", heapAllocTypeAddMD);
}

void addDMATypeMetadata(llvm::Module &M) {
    if (!_DMAType) {
        ERROR("DMATypeAdd is not initialized.");
        return;
    }

    // Process simple DMA type additions
    for (auto &simple_item : _DMAType->_simpleDMATypeAdd) {
        llvm::Function* caller = M.getFunction(simple_item._caller_name);
        if (!caller) {
            WARNING("Can't find caller function: " << simple_item._caller_name);
            continue;
        }
        
        // Collect all potential DMA allocation calls in the function
        std::vector<llvm::CallBase*> dmaAllocCalls;
        for (auto &BB : *caller) {
            for (auto &I : BB) {
                if (auto *CI = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    if (!CI->isIndirectCall()) {
                        llvm::Function* calledFunc = CI->getCalledFunction();
                        if (calledFunc) {
                            std::string funcName = calledFunc->getName().str();
                            // Check for DMA allocation functions
                            if (funcName == "malloc" || funcName == "calloc" || funcName == "realloc" || 
                                funcName == "_Znwm" || funcName == "_Znam") {
                                dmaAllocCalls.push_back(CI);
                            }
                        }
                    }
                }
            }
        }
        
        // Apply metadata to specified call sites
        for (auto &callsite_info : simple_item._callSiteInfoMap) {
            uint32_t callSiteID = callsite_info.first;
            const CallSiteInfo &csInfo = callsite_info.second;
            
            if (callSiteID < dmaAllocCalls.size()) {
                llvm::CallBase* targetCall = dmaAllocCalls[callSiteID]; // get the call at the specified index
                
                uint64_t typeHash = 0;
                if (csInfo.isBaseType) {
                    typeHash = llvm::stable_hash_combine_string(csInfo.DMAType);
                } else {
                    typeHash = computeStructHash(M, csInfo.DMAType);  // 修改这里的调用
                }
                
                applyTypeHashMetadata(targetCall, csInfo.DMAType, csInfo.isBaseType, typeHash);
            } else {
                WARNING("Call site ID " << callSiteID << " out of range in function " << simple_item._caller_name);
            }
        }
    }

    // Process complex DMA type additions (caller-callee pairs)
    for (auto &complex_item : _DMAType->_complexDMATypeAdd) {
        llvm::Function* caller = M.getFunction(complex_item._caller_name);
        llvm::Function* callee = M.getFunction(complex_item._callee_name);
        
        if (!caller) {
            WARNING("Can't find caller function: " << complex_item._caller_name);
            continue;
        }
        if (!callee) {
            WARNING("Can't find callee function: " << complex_item._callee_name);
            continue;
        }
        
        // Find all calls from caller to callee
        std::vector<llvm::CallBase*> callerToCalleeCalls;
        for (auto &BB : *caller) {
            for (auto &I : BB) {
                if (auto *CI = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    if (!CI->isIndirectCall() && CI->getCalledFunction() == callee) {
                        callerToCalleeCalls.push_back(CI);
                    }
                }
            }
        }
        
        // Apply metadata to specified call sites
        for (auto &callsite_info : complex_item._callSiteInfoMap) {
            uint32_t callSiteID = callsite_info.first;
            const CallSiteInfo &csInfo = callsite_info.second;
            
            if (callSiteID < callerToCalleeCalls.size()) {
                llvm::CallBase* targetCall = callerToCalleeCalls[callSiteID];
                
                uint64_t typeHash = 0;
                if (csInfo.isBaseType) {
                    typeHash = llvm::stable_hash_combine_string(csInfo.DMAType);
                } else {
                    typeHash = computeStructHash(M, csInfo.DMAType);
                }
                
                applyTypeHashMetadata(targetCall, csInfo.DMAType, csInfo.isBaseType, typeHash);
            } else {
                WARNING("Call site ID " << callSiteID << " out of range for " << 
                       complex_item._caller_name << " -> " << complex_item._callee_name);
            }
        }
    }

    // Process encapsulated DMA function call chains
    // For each call chain, we need to mark all called functions with metadata (!DMATypeArg)
    // This ensures type information can be passed through the function call chain via parameters
    for (auto &call_chain : _DMAType->_encapsulatedDMAFuncs) {
        // Extract caller-callee pairs from the call chain
        // For chain [A, B, C, D], we get pairs: (A,B), (B,C), (C,D)
        std::set<std::pair<std::string, std::string>> call_chain_pairs;
        for (size_t i = 0; i < call_chain.size() - 1; ++i) {
            std::string caller_name = call_chain[i];
            std::string callee_name = call_chain[i + 1];
            call_chain_pairs.emplace(caller_name, callee_name);
        }
        
        // Mark each caller-callee relationship in the chain
        for (const auto &pair : call_chain_pairs) {
            llvm::Function* caller = M.getFunction(pair.first);
            llvm::Function* callee = M.getFunction(pair.second);
            
            if (!caller || !callee) {
                WARNING("Can't find function in call chain: " << pair.first << " -> " << pair.second);
                continue;
            }
            
            // Find all call sites where caller calls callee
            std::vector<llvm::CallBase*> callerToCalleeCalls;
            for (auto &BB : *caller) {
                for (auto &I : BB) {
                    if (auto *CI = llvm::dyn_cast<llvm::CallBase>(&I)) {
                        if (!CI->isIndirectCall() && CI->getCalledFunction() == callee) {
                            callerToCalleeCalls.push_back(CI);
                        }
                    }
                }
            }
            
            // Mark all call sites with DMATypeArg metadata
            // This metadata indicates that these calls should pass type hash as an additional argument
            // The instrumenter will later create _hook versions of these functions with typeHash parameter
            for (auto *targetCall : callerToCalleeCalls) {
                llvm::LLVMContext &ctx = M.getContext();
                llvm::MDNode* dmaTypeArgMD = llvm::MDNode::get(ctx, 
                    {llvm::MDString::get(ctx, "true")});
                targetCall->setMetadata("DMATypeArg", dmaTypeArgMD);
                
                INFO("Marked call site in " << pair.first << " -> " << pair.second << 
                     " for DMA type argument passing");
            }
        }
    }
    
    INFO("DMA type metadata addition completed");
}

// Prioritize the Type Metadata results and take the intersection of SVF and DeepType results.
// If a manual-recognized incall target is specified, it is preferred.
void mergeIncallEdges() {
    for (auto &pair : TypeMDInCallEdges) {
        incallEdges[pair.first] = pair.second;
    }
    
    for (auto &pair : SVFInCallEdges) {
        int callID = pair.first;
        
        if (TypeMDInCallEdges.find(callID) != TypeMDInCallEdges.end()) {
            continue;
        }
        
        if (DeepTypeInCallEdges.find(callID) != DeepTypeInCallEdges.end()) {
            inCallTargets mergedTargets;
            mergedTargets.isVCall = pair.second.isVCall && DeepTypeInCallEdges[callID].isVCall;
            for (const auto &target : pair.second.targets) {
                if (DeepTypeInCallEdges[callID].targets.find(target) != DeepTypeInCallEdges[callID].targets.end()) {
                    mergedTargets.targets.insert(target);
                }
            }
            
            if (!mergedTargets.targets.empty()) {
                incallEdges[callID] = mergedTargets;
            }
        }
    }

    for (auto &pair : manualInCallEdges){
        incallEdges[pair.first] = pair.second;
    }
}

void addTargetsMD(llvm::Module &M, bool printUnprocessed = false) {
    for (auto &F : M) {
        for (auto &I : instructions(F)) {
            if (auto *CB = dyn_cast<CallBase>(&I)) {
                if (auto *MD = CB->getMetadata("inCallID")) {
                    auto *md = dyn_cast<MDNode>(MD);
                    int ID = cast<ConstantInt>(cast<ConstantAsMetadata>(md->getOperand(0))->getValue())->getZExtValue();
                    incallID2CallBase[ID] = CB;
                }
            }
        }
    }

    for (auto &pair : incallEdges){
        int callID = pair.first;
        auto &callTarget = pair.second;
        auto &targets = callTarget.targets;
        std::vector<llvm::Metadata*> targetMDs;
        for (auto &target : targets){
            targetMDs.push_back(llvm::MDString::get(M.getContext(), target));
        }
        if (targetMDs.empty()){
            continue;
        }
        llvm::MDNode *TargetsMD = llvm::MDNode::get(M.getContext(), targetMDs);
        incallID2CallBase[pair.first]->setMetadata("targets", TargetsMD);
    }

    if (printUnprocessed) {
        INFO("################## Unprocessed Indirect Calls ##################");
        uint32_t totalMiss = 0;
        for (auto &pair : incallID2CallBase){
            if (incallEdges.find(pair.first) == incallEdges.end()){
                totalMiss++;
                INFO("---------------------------------");
                INFO("Indircect Call ID: " << pair.first);
                LLVM_INFO("Indirect Call Site: " << *pair.second);
                LLVM_INFO("Function: " << pair.second->getCaller()->getName());
            }
        }
        INFO("---------------------------------");
        INFO("Total unprocessed indirect calls: " << totalMiss);
        INFO("###############################################################");
    }
}

int main(int argc, char **argv) {
    cl::SubCommand AddMDMode("add-md", "Add targets metadata to IR file");
    cl::SubCommand MergeMode("merge", "Merge indirect call results");
    // add-md
    cl::opt<std::string> InputIRFileName(cl::Positional, 
                                        cl::desc("<input IR file>"), 
                                        cl::Required,
                                        cl::sub(AddMDMode));
    cl::opt<std::string> InputYAMLFilePath("incall-config", 
                                          cl::desc("Specify input Incall Map YAML file"), 
                                          cl::value_desc("filename"), 
                                          cl::Required,
                                          cl::sub(AddMDMode));
    cl::opt<std::string> DMATypeAddYamlPath("dma-config", 
                                           cl::desc("Specify DMA type YAML file"), 
                                           cl::value_desc("filename"), 
                                           cl::Optional,
                                           cl::sub(AddMDMode));
    cl::opt<bool> PrintUnprocessed("p", 
                                  cl::desc("Print statistics about unprocessed indirect calls"), 
                                  cl::init(false),
                                  cl::sub(AddMDMode));
    
    // merge
    cl::opt<std::string> MergeInputDir("dir", 
                                       cl::desc("Specify directory containing YAML files to merge"), 
                                       cl::value_desc("directory"), 
                                       cl::Required,
                                       cl::sub(MergeMode));
    cl::opt<std::string> ManualInCallsFile("manual", 
                                           cl::desc("Specify manual identified indirect calls YAML file"), 
                                           cl::value_desc("filename"), 
                                           cl::Optional,
                                           cl::sub(MergeMode));

    cl::ParseCommandLineOptions(argc, argv, "Indirect call targets utility\n");

    if (AddMDMode) {
        LLVMContext Context;
        SMDiagnostic Err;

        std::unique_ptr<Module> M = parseIRFile(InputIRFileName, Err, Context);
        if (!M) {
            Err.print(argv[0], errs());
            return 1;
        }

        parseYAML(InputYAMLFilePath, incallEdges);
        addTargetsMD(*M, PrintUnprocessed);

        if (!DMATypeAddYamlPath.empty()) {
            _DMAType = std::make_unique<DMATypeAdd>(parseDMATypeYAML(DMATypeAddYamlPath));
            addDMATypeMetadata(*M);
        }

        std::error_code EC;
        llvm::raw_fd_ostream OS(InputIRFileName, EC, llvm::sys::fs::OF_None);  
        if (EC) {
            LLVM_ERROR("Could not open file: " << EC.message());
            return 1;
        }
        M->print(OS, nullptr);

        INFO("Add targets metadata to IR file " << InputIRFileName << " successfully!");
    } 
    else if (MergeMode) {
        llvm::outs() << "[INFO] Merging indirect call results...\n";
        // Check if the directory exists
        if (!llvm::sys::fs::exists(MergeInputDir)) {
            ERROR("Directory " << MergeInputDir << " does not exist!");
            return 1;
        }
        std::string TypeMDInCalls = MergeInputDir + "/TypeMDInCalls.yml";
        std::string SVFInCalls = MergeInputDir + "/SVFInCalls.yml";
        std::string DeepTypeInCalls = MergeInputDir + "/DeepTypeInCalls.yml";

        bool hasInCallFile = false;
        if (llvm::sys::fs::exists(TypeMDInCalls)) {
            hasInCallFile = true;
            parseYAML(TypeMDInCalls, TypeMDInCallEdges);
        }
        if (llvm::sys::fs::exists(SVFInCalls)) {
            hasInCallFile = true;
            parseYAML(SVFInCalls, SVFInCallEdges);
        }
        if (llvm::sys::fs::exists(DeepTypeInCalls)) {
            hasInCallFile = true;
            parseYAML(DeepTypeInCalls, DeepTypeInCallEdges);
        }
        
        if (!ManualInCallsFile.empty()) {
            if (llvm::sys::fs::exists(ManualInCallsFile)) {
                parseYAML(ManualInCallsFile, manualInCallEdges);
                INFO("Loaded manual indirect call targets from " << ManualInCallsFile);
            } else {
                WARNING("Manual indirect call file " << ManualInCallsFile << " does not exist!");
            }
        }

        if (!hasInCallFile && manualInCallEdges.empty()) {
            ERROR("No indirect call results found");
            ERROR("Please provide at least one valid indirect call file");
            return 1;
        }

        mergeIncallEdges();

        std::string outputFilePath = "indirectCalls.yml";
        if (!writeYAMLFile(outputFilePath, incallEdges)) {
            return 1;
        }

        INFO("Merged indirect call results written to " << outputFilePath);
    }
    else {
        ERROR("Please specify a mode: add-md or merge");
        return 1;
    }
    
    return 0;
}