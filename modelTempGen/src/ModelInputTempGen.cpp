#include "modelInputTempGen.h"

std::string llvmBaseTypeToString(llvm::Type* type) {
    std::string ret = "";
    if (!type){
        LLVM_ERROR("Type is null.\n");
        exit(1);
    }
    if(type->isFloatTy()){
        ret = "float";
    }else if(type->isDoubleTy()){
        ret = "double";
    }else if(type->isIntegerTy()){
        ret = "i" + std::to_string(type->getIntegerBitWidth()); // e.g. i32
    }else if(type->isPointerTy()){
        ret = "ptr";
    }else{
        LLVM_ERROR("Unsupport llvm type: " << *type << "\n");
        exit(1);
    }
    return ret;
}

modelInputs::TypeSpec llvmBaseTypeToTypeSpec(llvm::Type* type) {
    modelInputs::TypeSpec ret;
    if (!type){
        LLVM_ERROR("Type is null.\n");
        exit(1);
    }
    if(type->isFloatTy()){
        ret = modelInputs::TypeSpec::FLOAT;
    }else if(type->isDoubleTy()){
        ret = modelInputs::TypeSpec::DOUBLE;
    }else if(type->isIntegerTy()){
        ret = modelInputs::TypeSpec::INT;
    }else if(type->isPointerTy()){
        ret = modelInputs::TypeSpec::PTR;
    }else{
        LLVM_ERROR("Unsupport llvm type: " << *type << "\n");
        exit(1);
    }
    return ret;
}

const uint64_t baseAddress = 0x4000000; // Base address for global variables.

PreservedAnalyses ModelInputTempGen::run(Module &M, ModuleAnalysisManager &MAM) {
    mapGVToMemAddr(M);
    mapHeapTypeHashToType(M);
    dumpBinary(M);
    return PreservedAnalyses::all();
}

void ModelInputTempGen::mapGVToMemAddr(llvm::Module &M) {
    
    uint64_t currentOffset = 0;
    for (GlobalVariable &GV : M.globals()) {

        if (GV.isConstant() || GV.getName().startswith("llvm.") || GV.isDeclaration()) {
            continue;
        }

        std::string section_name = "";
        llvm::MDNode *MDNode = GV.getMetadata("globalVarID");
        if (MDNode && MDNode->getNumOperands() > 0) {
            auto *MDInt = llvm::dyn_cast<llvm::ConstantAsMetadata>(MDNode->getOperand(0));
            if (MDInt) {
                int globalVarID = llvm::cast<llvm::ConstantInt>(MDInt->getValue())->getZExtValue();
                section_name = "global_vars." + std::to_string(globalVarID);
            }
        }

        // Only process global variables within global_vars section.
        if ("" == section_name || GV.getSection() != section_name) {
            continue;
        }

        uint64_t size = M.getDataLayout().getTypeAllocSize(GV.getValueType());
        uint64_t alignment = GV.getAlignment();
        if (alignment == 0) {
            alignment = M.getDataLayout().getABITypeAlign(GV.getValueType()).value();
        }

        currentOffset = llvm::alignTo(currentOffset, alignment);    // Align the offset
        
        uint64_t physicalAddress = baseAddress + currentOffset;
        
        _gvAddrMap.push_back(std::make_pair(&GV, physicalAddress));
        
        currentOffset += size;
    }
}

void ModelInputTempGen::mapHeapTypeHashToType(llvm::Module &M) {
    // First find all !heapAllocType and !heapAllocTypeAdd metadata's hash values
    std::vector<std::pair<std::string, uint64_t>> typeNameHashPairs;
    for (auto &F : M) {
        if (F.isDeclaration() || F.getName().startswith("dummyAllocSTy.")) {
            continue;
        }
        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *callInst = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    // handle !heapAllocType 
                    if (auto *heapAllocType = callInst->getMetadata("heapAllocType")) {
                        auto *typeStrMD = dyn_cast<MDString>(heapAllocType->getOperand(0));
                        auto *hashMD = dyn_cast<ConstantAsMetadata>(heapAllocType->getOperand(2));
                        auto *hashConst = dyn_cast<ConstantInt>(hashMD->getValue());
                        std::string typeName = typeStrMD->getString().str();
                        uint64_t hashValue = hashConst->getZExtValue();
                        typeNameHashPairs.push_back(std::make_pair(typeName, hashValue));
                    }
                    
                    // handle !heapAllocTypeAdd
                    if (auto *heapAllocTypeAdd = callInst->getMetadata("heapAllocTypeAdd")) {
                        auto *typeStrMD = dyn_cast<MDString>(heapAllocTypeAdd->getOperand(0));
                        auto *hashMD = dyn_cast<ConstantAsMetadata>(heapAllocTypeAdd->getOperand(2));
                        auto *hashConst = dyn_cast<ConstantInt>(hashMD->getValue());
                        std::string typeName = typeStrMD->getString().str();
                        uint64_t hashValue = hashConst->getZExtValue();
                        typeNameHashPairs.push_back(std::make_pair(typeName, hashValue));
                    }
                }
            }
        }
    }

    // fill _heapVarTypeMap
    for (auto &pair : typeNameHashPairs) {
        std::string typeName = pair.first;
        uint64_t hashValue = pair.second;

        // Check if the type hash is already in the map
        if (_heapVarTypeMap.find(hashValue) == _heapVarTypeMap.end()) {
            // If not, create a new type based on the name
            llvm::Type *newType = nullptr;
            if (typeName == "i8") {
                newType = Type::getInt8Ty(M.getContext());
            } 
            else if (typeName == "i16") {
                newType = Type::getInt16Ty(M.getContext());
            } 
            else if (typeName == "i32") {
                newType = Type::getInt32Ty(M.getContext());
            } 
            else if (typeName == "i64") {
                newType = Type::getInt64Ty(M.getContext());
            } 
            else if (typeName == "float") {
                newType = Type::getFloatTy(M.getContext());
            } 
            else if (typeName == "double") {
                newType = Type::getDoubleTy(M.getContext());
            } 
            else if (typeName == "ptr") {
                newType = Type::getInt8PtrTy(M.getContext());
            } 
            // next handle struct type
            else {
                for (auto &ST : M.getIdentifiedStructTypes()) {
                    if (ST->isOpaque() || ST->getName().empty()) {
                        continue;
                    }
                    
                    uint64_t STyHashValue = hashType(ST);

                    if (STyHashValue == hashValue) {
                        newType = ST;
                        break;
                    }
                }
                if (!newType) {
                    LLVM_ERROR("Unknown type: " << typeName << " with hash value: " << hashValue);
                    exit(1);
                }
            }
            _heapVarTypeMap[hashValue] = newType;
        }
    }
}


void ModelInputTempGen::dumpBinary(Module &M){
    // if the output directory not exists, create it
    if (!std::filesystem::exists(_tmpOutputPath)) {
        std::filesystem::create_directories(_tmpOutputPath);
    }

    modelInputs::ModelInputs gvInputs;
    fillGlobalVar(M, gvInputs);
    modelInputs::ModelInputs heapInputs;
    fillHeapVar(M, heapInputs);

    std::ofstream file(_tmpOutputPath + _fcType + "_modelInGV.tmp", std::ios::binary);
    if (!file.is_open()) {
        LLVM_ERROR("Failed to open output file " + _fcType + "_modelInGV.tmp\n");
        exit(1);
    }
    if (!gvInputs.SerializeToOstream(&file)) {
        LLVM_ERROR("Failed to serialize global vars to binary file.\n");
        exit(1);
    }
    file.close();
    INFO("Model global variables input template binary file generated successfully.");

    std::ofstream file2(_tmpOutputPath + _fcType + "_modelInHeap.tmp", std::ios::binary);
    if (!file2.is_open()) {
        LLVM_ERROR("Failed to open output file " + _fcType + "_modelInHeap.tmp\n");
        exit(1);
    }
    if (!heapInputs.SerializeToOstream(&file2)) {
        LLVM_ERROR("Failed to serialize heap vars to binary file.\n");
        exit(1);
    }
    file2.close();
    INFO("Model heap variables input template binary file generated successfully.");
}

size_t ModelInputTempGen::hashType(llvm::Type* Ty) {
    switch (Ty->getTypeID()) {
        case llvm::Type::VoidTyID:      return stable_hash_combine_string("void");
        case llvm::Type::HalfTyID:      return stable_hash_combine_string("half");
        case llvm::Type::BFloatTyID:    return stable_hash_combine_string("bfloat");
        case llvm::Type::FloatTyID:     return stable_hash_combine_string("float");
        case llvm::Type::DoubleTyID:    return stable_hash_combine_string("double");
        case llvm::Type::X86_FP80TyID:  return stable_hash_combine_string("x86_fp80");
        case llvm::Type::FP128TyID:     return stable_hash_combine_string("fp128");
        case llvm::Type::PPC_FP128TyID: return stable_hash_combine_string("ppc_fp128");
        case llvm::Type::LabelTyID:     return stable_hash_combine_string("label");
        case llvm::Type::MetadataTyID:  return stable_hash_combine_string("metadata");
        case llvm::Type::X86_MMXTyID:   return stable_hash_combine_string("x86_mmx");
        case llvm::Type::X86_AMXTyID:   return stable_hash_combine_string("x86_amx");
        case llvm::Type::TokenTyID:     return stable_hash_combine_string("token");
        case llvm::Type::IntegerTyID: {
            std::string intType = "i" + std::to_string(cast<llvm::IntegerType>(Ty)->getBitWidth());
            return stable_hash_combine_string(intType);
        }
        case llvm::Type::PointerTyID:   return stable_hash_combine_string("ptr");
        case llvm::Type::FunctionTyID: {
            llvm::FunctionType *FTy = cast<llvm::FunctionType>(Ty);
            size_t retHash = hashType(FTy->getReturnType());
            for (Type *paramTy : FTy->params()) {
                retHash = stable_hash_combine(retHash, hashType(paramTy));
            }
            StringRef varArg = FTy->isVarArg() ? "varArg:true" : "varArg:false";
            return llvm::stable_hash_combine(retHash, stable_hash_combine_string(varArg));
          }
        case llvm::Type::StructTyID: {
            llvm::StructType *STy = cast<llvm::StructType>(Ty);
        
            if (!STy->isLiteral()) {
                size_t nameHash = stable_hash_combine_string(getStructTypeNamePrefix(STy->getName()));
                StringRef packed = STy->isPacked() ? "packed:true" : "packed:false";
                size_t retHash = stable_hash_combine_string(packed);
                for (llvm::Type *elemTy : STy->elements()) {
                    nameHash = llvm::stable_hash_combine(nameHash, hashType(elemTy));
                }
                return llvm::stable_hash_combine(nameHash, retHash);
            }else{
                StringRef packed = STy->isPacked() ? "packed:true" : "packed:false";
                size_t retHash = stable_hash_combine_string(packed);
                for (llvm::Type *elemTy : STy->elements()) {
                    retHash = llvm::stable_hash_combine(retHash, hashType(elemTy));
                }
                return retHash;
            }
        }
        case llvm::Type::ArrayTyID: {
            llvm::ArrayType *ATy = cast<llvm::ArrayType>(Ty);
            size_t elemHash = hashType(ATy->getElementType());
            std::string STStr;
            llvm::raw_string_ostream STStream(STStr);
            STStream << ATy->getNumElements();
            return llvm::stable_hash_combine(elemHash, stable_hash_combine_string(STStream.str()));
        }
        case llvm::Type::FixedVectorTyID:
        case llvm::Type::ScalableVectorTyID: {
            llvm::VectorType *PTy = cast<llvm::VectorType>(Ty);
            llvm::ElementCount EC = PTy->getElementCount();
            size_t elemHash = hashType(PTy->getElementType());
            if (EC.isScalable()) {
                return llvm::stable_hash_combine(elemHash, stable_hash_combine_string("vscale"));
            } else {
                std::string STStr;
                llvm::raw_string_ostream STStream(STStr);
                STStream << EC.getKnownMinValue();
                return llvm::stable_hash_combine(elemHash, stable_hash_combine_string(STStream.str()));
            }
        }
        case llvm::Type::TypedPointerTyID: {
            llvm::TypedPointerType *TPTy = cast<llvm::TypedPointerType>(Ty);
            size_t elemHash = hashType(TPTy->getElementType());
            return llvm::stable_hash_combine(stable_hash_combine_string("typedptr"), elemHash);
        }
        case llvm::Type::TargetExtTyID: {
            llvm::TargetExtType *TETy = cast<llvm::TargetExtType>(Ty);
            std::string STStr;
            llvm::raw_string_ostream STStream(STStr);
            STStream << "target(\"";
            printEscapedString(Ty->getTargetExtName(), STStream);
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

StringRef ModelInputTempGen::getStructTypeNamePrefix(StringRef Name) {
    while (!Name.empty()) {
      size_t DotPos = Name.rfind('.');
      if (DotPos == StringRef::npos || DotPos == 0 || DotPos == Name.size() - 1)
        return Name;
        
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

void ModelInputTempGen::fillGlobalVar(Module &M, modelInputs::ModelInputs& gvInputs) {
    uint32_t offset = 0;
    uint32_t totalSize = 0;
    for (auto &kv : _gvAddrMap) {
        GlobalVariable *GV = kv.first;
        uint64_t addr = kv.second;
        uint32_t gvSize = M.getDataLayout().getTypeAllocSize(GV->getValueType());
        // we only consider aggregate type(struct and array) and base type(integer, float, double, pointer)
        modelInputs::GlobalVar *gv = gvInputs.add_globalvars();
        gv->set_name(GV->getName().str());
        gv->set_addr(addr);
        gv->set_size(gvSize);
        gv->set_offset(offset);
        if (!GV->getValueType()->isAggregateType()) {
            gv->set_type(llvmBaseTypeToTypeSpec(GV->getValueType()));
            offset += gvSize;
        }else {
            gv->set_type(modelInputs::TypeSpec::AGGR);
            fillAggrMember<modelInputs::GlobalVar>(M, gv, GV->getValueType(), offset);
        }
        totalSize += gvSize;
    }
    INFO("Total size of global variables: " << totalSize << " bytes");
}

template <typename T>
void ModelInputTempGen::fillAggrMember(Module &M, T* var, llvm::Type* aggrType, uint32_t &startOffset) {
    std::vector<std::pair<llvm::Type*, uint32_t>> stack;
    stack.push_back(std::make_pair(aggrType, startOffset));
    while (!stack.empty()) {
        auto [type, offset] = stack.back();
        stack.pop_back();
        if (llvm::StructType *ST = llvm::dyn_cast<llvm::StructType>(type)) {
            for (int i = ST->getNumElements() - 1; i >= 0; i--) {
                stack.push_back(std::make_pair(ST->getElementType(i), offset + M.getDataLayout().getStructLayout(ST)->getElementOffset(i)));
            }
        } else if (llvm::ArrayType *AT = llvm::dyn_cast<llvm::ArrayType>(type)) {
            for (int i = AT->getNumElements() - 1; i >= 0; i--) {
                stack.push_back(std::make_pair(AT->getElementType(), offset + i * M.getDataLayout().getTypeAllocSize(AT->getElementType())));
            }
        } else if (type->isPointerTy() || type->isIntegerTy() || type->isFloatTy() || type->isDoubleTy()) {
            auto* memberList = var->mutable_members();
            auto* member = memberList->add_members();
            member->set_member_offset(offset - startOffset);
            member->set_type(llvmBaseTypeToTypeSpec(type)); 
            member->set_size(M.getDataLayout().getTypeAllocSize(type));  
        } else {
            LLVM_ERROR("Unknown element: " << *type << "\n");
            exit(1);
        }
    }
    startOffset += M.getDataLayout().getTypeAllocSize(aggrType);
}

void ModelInputTempGen::fillHeapVar(Module &M, modelInputs::ModelInputs& heapInputs) {
    uint32_t offset = 0;
    for (auto &kv : _heapVarTypeMap) {
        auto &hashValue = kv.first;
        auto &type = kv.second;
        modelInputs::HeapVar* hv = heapInputs.add_heapvars();
        hv->set_hash(hashValue);
        if (type->isAggregateType()) {
            hv->set_type(modelInputs::TypeSpec::AGGR);
            hv->set_size(M.getDataLayout().getTypeAllocSize(type));
            fillAggrMember<modelInputs::HeapVar>(M, hv, type, offset);
        } else if (type->isIntegerTy() || type->isFloatTy() || type->isDoubleTy() || type->isPointerTy()) {
            hv->set_type(llvmBaseTypeToTypeSpec(type));
            hv->set_size(M.getDataLayout().getTypeAllocSize(type));
            offset += M.getDataLayout().getTypeAllocSize(type);
        } else {
            LLVM_ERROR("Unknown type: " << *type << "\n");
            exit(1);
        }
    }
}

void runMITGen(Module &M, std::string tmp_output_path, std::string fc_type) {
    llvm::PassBuilder PB;
    llvm::ModulePassManager MPM;
    llvm::ModuleAnalysisManager MAM;

    // Register analysis passes with the managers
    PB.registerModuleAnalyses(MAM);
    ModelInputTempGen MIGen(tmp_output_path, fc_type);
    MPM.addPass(std::move(MIGen));
    MPM.run(M, MAM);
}