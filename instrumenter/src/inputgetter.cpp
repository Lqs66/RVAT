#include "instrumenter.hh"
#include <iostream>
#include <queue>

using namespace llvm;


/**
 * @brief for debug
 */
#include <fstream>
void dumpGlobalVars(std::string output_file, std::vector<std::pair<std::string, int>> global_vars){
    std::ofstream out(output_file);
    if (!out.is_open()) {
        ERROR("Could not open file: " << output_file);
        return;
    }
    int offset = 0;
    for (auto& it : global_vars) {
        out << it.first << ": " << offset << std::endl;
        offset += it.second;
    }
    out.close();
}

void instrumenter::Inputgetter::insertDumpVarsFuncCall(llvm::Function& F){
    llvm::Module &M = *F.getParent();
    llvm::Function* dumpFunc = M.getFunction("dump_vars");

    std::vector<llvm::Value*> args;
    for (auto &arg : F.args()) {
        args.push_back(&arg);
    }
    if (dumpFunc) {
        llvm::BasicBlock &entry = F.getEntryBlock();
        llvm::Instruction *firstNonPhi = entry.getFirstNonPHI();
        llvm::CallInst::Create(dumpFunc, args, "", firstNonPhi);
    } else {
        ERROR("dump_vars() is not found");
    }
}

// we insert call global_init_help() in the beginning of the entrypoint function
void instrumenter::Inputgetter::insertGlobalInitCode(llvm::Function* entrypoint){
    llvm::Module* M = entrypoint->getParent();
    llvm::Function* globalInitHelper = M->getFunction("global_init_help");
    if (!globalInitHelper)
        globalInitHelper = createGlobalInitHelper(*M);
    llvm::BasicBlock& entry = entrypoint->getEntryBlock();
    llvm::Instruction* firstNonPhi = entry.getFirstNonPHI();
    llvm::IRBuilder builder(firstNonPhi);
    builder.CreateCall(globalInitHelper);
}

/// @brief According to _bbNumAndNoInlinedIDsMap, we fill _bbAndNoInlinedCallsMap.
///        Convert ID to llvm object.
///        _bbNumAndNoInlinedIDsMap: key is bbNum, value is a set of no inlined ids.
///        _bbAndNoInlinedCallsMap: key is BasicBlock*, value is a vector of CallBase*.
void instrumenter::Inputgetter::fillTargets(llvm::Module &M){

    for (auto &F : M){
        if (F.isDeclaration()) {
            continue;
        }
        if (F.getName().startswith("dummyAllocSTy.")) {
            continue;
        }
        for (auto &BB : F){
            uint32_t bbNum = 0;
            try{
                bbNum = std::stoi(BB.getName().str().substr(5));
            } catch (std::invalid_argument &e){
                ERROR("Invalid argument: " << e.what());
                ERROR("BB Name: " << BB.getName().str());
                exit(1);
            } catch (std::out_of_range &e){
                ERROR("Out of range: " << e.what());
                ERROR("BB Name: " << BB.getName().str());
                exit(1);
            }
            if (_bbNumAndNoInlinedIDsMap.count(bbNum) > 0){
                for (uint32_t id : _bbNumAndNoInlinedIDsMap[bbNum]){
                    // we get all callsites in BB
                    std::set<llvm::CallBase*> callsites;
                    for (auto &inst : BB){
                        if (auto callInst = llvm::dyn_cast<llvm::CallBase>(&inst)){
                            callsites.insert(callInst);
                        }
                    }
                    if (id >= 0x80000000){
                        id &= 0x7fffffff;
                        for (auto &callInst : callsites){
                            if (callInst->isIndirectCall()){
                                if (getUID(callInst) == id){
                                    _bbAndNoInlinedCallsMap[&BB].push_back(callInst);
                                }
                            }
                        }
                    } else {
                        for (auto &callInst : callsites){
                            if (!callInst->isIndirectCall()){
                                if (getUID(callInst->getCalledFunction()) == id){
                                    _bbAndNoInlinedCallsMap[&BB].push_back(callInst);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // std::set<llvm::Function*> targetF;
    std::set<llvm::CallBase*> targetCB;
    for (auto &pair : _bbAndNoInlinedCallsMap){
        for (auto &callInst : pair.second){
            // if (callInst->getType()->isVoidTy()){
            //     continue;
            // }
            targetCB.insert(callInst);
            // if (!callInst->isIndirectCall()){
            //     targetF.insert(callInst->getCalledFunction());
            // }
        }
    }
    // INFO("Num of target functions: " << targetF.size());
    INFO("Num of parse callsites: " << targetCB.size());
}

/// @brief According to _bbAndNoInlinedCallsMap, we insert instrumentation to obtain return values immediately after these call instructions. 
// COMMENTED: Function return value collection feature is disabled
/*
void instrumenter::Inputgetter::insertCallRetValCollector(llvm::Function* entrypoint){
    // retValsBinFile: the path of the bin file
    // The format of this file is:
    // The binary file starts with a 4-byte integer, where the high bit is the is_direct flag (0 or 1), the second bit is the is_ptr flag (0 or 1), and the low 30 bits are the id. Followed by 4 bytes of size, and then size bytes of return value.
    // For example: [4-byte val: is_direct|is_ptr|id] [4-byte size] [size bytes data]
    llvm::Module* M = entrypoint->getParent();
    insertGlobalInitCode(entrypoint);
    llvm::Function* fwriteFunc = M->getFunction("fwrite");
    if (!fwriteFunc) {
        fwriteFunc = createFwriteFunc(*M);
    }
    int totalInsert = 0;
    std::set<llvm::Function*> ignoreFuncs;
    std::set<llvm::CallBase*> ignoreCallsites;
    for (auto &pair : _bbAndNoInlinedCallsMap) {
        llvm::BasicBlock* bb = pair.first;
        llvm::Function* caller = bb->getParent();
        std::vector<llvm::CallBase*> targetCalls = pair.second;
        std::map<llvm::Type*, llvm::AllocaInst*> allocaMap;
        llvm::BasicBlock& entry = caller->getEntryBlock();
        llvm::Instruction* firstNonPhi = entry.getFirstNonPHI();
        llvm::IRBuilder<> builder(firstNonPhi);

        // we alloca retType to store return value
        for (auto &callInst : targetCalls) {
            // Do not collect return values of intrinsic functions ,math functions and dynamic memory alloc functions.
            if (auto callee = callInst->getCalledFunction()){
                if (callee->isIntrinsic() || 
                    std::find(utils::mathFuncs.begin(), utils::mathFuncs.end(), callee->getName().str()) != utils::mathFuncs.end() ||
                    std::find(utils::dmaFuncs.begin(), utils::dmaFuncs.end(), callee->getName().str()) != utils::dmaFuncs.end()
                ){
                    continue;
                }
            }
            llvm::Type* retType = callInst->getType();
            if (retType->isVoidTy()) {
                continue;
            }
            if (allocaMap.find(retType) == allocaMap.end()) {
                allocaMap[retType] = builder.CreateAlloca(retType, nullptr, "RetAlloca");
            }
        }
        // we insert call retValCollector(id, &retValue, sizeof(retValue)) after the call instruction
        for (auto &callInst : targetCalls){
            // Do not collect return values of intrinsic functions ,math functions and dynamic memory alloc functions.
            if (auto callee = callInst->getCalledFunction()){
                if (callee->isIntrinsic() || std::find(utils::mathFuncs.begin(), utils::mathFuncs.end(), callee->getName().str()) != utils::mathFuncs.end() ||
                std::find(utils::dmaFuncs.begin(), utils::dmaFuncs.end(), callee->getName().str()) != utils::dmaFuncs.end()
            ){
                    ignoreCallsites.insert(callInst);
                    continue;
                }
            }
            if (callInst->getType()->isVoidTy()) {
                ignoreCallsites.insert(callInst);
                continue;
            }
            builder.SetInsertPoint(callInst->getNextNode());
            // if callInst is indirect call, we set the highest bit to 1; if return type is ptr, we set the second bit to 1
            // else: uid = or i32 0, xxx
            llvm::Value* uid = nullptr;
            if (callInst->isIndirectCall()) {
                uid = builder.CreateOr(llvm::ConstantInt::get(llvm::Type::getInt32Ty(M->getContext()), 0x80000000), llvm::ConstantInt::get(llvm::Type::getInt32Ty(M->getContext()), getUID(callInst)), "iid");
                // builder.CreateStore(uid, IDAllocaInst);
            } else {
                uid = builder.CreateOr(llvm::ConstantInt::get(llvm::Type::getInt32Ty(M->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(M->getContext()), getUID(callInst->getCalledFunction())), "fid");
                // builder.CreateStore(uid, IDAllocaInst);
            }
            if (callInst->getType()->isPointerTy()) {
                uid = builder.CreateOr(uid, llvm::ConstantInt::get(llvm::Type::getInt32Ty(M->getContext()), 0x40000000));
            }
            // we store the return value into RetAllocaInst
            // store retValue, ptr RetAllocaInst
            builder.CreateStore(callInst, allocaMap[callInst->getType()]);
            // we insert retValCollector() call behind the call instruction
            // call void retValCollector(i32 noundef uid, ptr noundef RetAllocaInst, i64 noundef size)
            llvm::Function* retValCollector = M->getFunction("retValCollector");
            if (!retValCollector) {
                retValCollector = createRetValCollector(*M);
            }
            llvm::CallInst* retValCollectorCall = builder.CreateCall(retValCollector, {uid, allocaMap[callInst->getType()], llvm::ConstantInt::get(llvm::Type::getInt64Ty(M->getContext()), M->getDataLayout().getTypeAllocSize(callInst->getType()))});
            totalInsert++;
        }
    }

    for (auto &CB : ignoreCallsites){
        if (auto callee = CB->getCalledFunction()){
            ignoreFuncs.insert(callee);
        }
    }
    INFO("Num of ignore functions: " << ignoreFuncs.size());
    INFO("Num of ignore callsites: " << ignoreCallsites.size());
    INFO("Num of total instrumented callsites: " << totalInsert);
}
*/  // End of commented insertCallRetValCollector

void instrumenter::ArduCopter::createGlobalParam(llvm::Module &M, ParamType ptype, std::string paramName, uint32_t param_id, int64_t initValue) {
    llvm::LLVMContext& ctx = M.getContext();
    std::string paramNameUpper = paramName;
    std::transform(paramNameUpper.begin(), paramNameUpper.end(), paramNameUpper.begin(), ::toupper);
    // we create a global string constant, it represents the parameter name
    llvm::GlobalVariable* paramNameGV = new llvm::GlobalVariable(
        M,
        llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), paramName.length() + 1),
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantDataArray::getString(ctx, paramNameUpper),
        paramName + "_str"
    );
    paramNameGV->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);

    // we according to the parameter type to create a global variable to represent the parameter
    // create struct type, its element is the parameter type
    llvm::Type* paramType = nullptr;
    std::string structName = "AP_Param";
    uint32_t type_id = 0;
    switch(ptype){
        case ParamType::I8:
            paramType = llvm::Type::getInt8Ty(ctx);
            structName += "I8";
            type_id = 1;
            break;
        case ParamType::I16:
            paramType = llvm::Type::getInt16Ty(ctx);
            structName += "I16";
            type_id = 2;
            break;
        case ParamType::I32:
            paramType = llvm::Type::getInt32Ty(ctx);
            structName += "I32";
            type_id = 3;
            break;
        default:
            ERROR("Unknown parameter type");
            return;
    }
    llvm::StructType* structType = llvm::StructType::create(ctx, {paramType}, structName);
    llvm::GlobalVariable* paramGV = new llvm::GlobalVariable(
        M,
        structType,
        false,
        llvm::GlobalValue::ExternalLinkage,
        llvm::ConstantStruct::get(structType, {llvm::ConstantInt::get(paramType, initValue)}),
        paramName
    );
    // paramGV->setSection("global_vars");

    // next we add new member to @_ZN6Copter8var_infoE
    // because it is a global constant, we need to create a new @_ZN6Copter8var_infoE to replace the old one
    llvm::StructType* newVarInfoMemberType = llvm::StructType::get(
        llvm::Type::getInt8PtrTy(ctx),
        llvm::Type::getInt8PtrTy(ctx), 
        llvm::StructType::get(ctx, {llvm::Type::getFloatTy(ctx), llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 4)}), 
        llvm::Type::getInt16Ty(ctx), 
        llvm::Type::getInt16Ty(ctx), 
        llvm::Type::getInt8Ty(ctx)
    );
    llvm::Constant* newVarInfoMemberInit = llvm::ConstantStruct::get(
        newVarInfoMemberType,
        paramNameGV,
        paramGV,
        llvm::ConstantStruct::get(llvm::StructType::get(ctx, {llvm::Type::getFloatTy(ctx), llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 4)}), 
                             {llvm::ConstantFP::get(llvm::Type::getFloatTy(ctx), static_cast<double>(initValue)), 
                              llvm::UndefValue::get(llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 4))}),
    llvm::ConstantInt::get(llvm::Type::getInt16Ty(ctx), 0),
    llvm::ConstantInt::get(llvm::Type::getInt16Ty(ctx), param_id),
    llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx), type_id)
    );
    llvm::GlobalVariable* oldVarInfoGV = M.getGlobalVariable("_ZN6Copter8var_infoE", true);
    assert(oldVarInfoGV != nullptr && "Can't find global variable @_ZN6Copter8var_infoE");
    // If we find it, we rename its name to _ZN6Copter8var_infoE_old.
    std::string oldVarInfoName = oldVarInfoGV->getName().str();
    oldVarInfoGV->setName("_ZN6Copter8var_infoE_old");
    llvm::StructType* oldVarInfoType = llvm::cast<llvm::StructType>(oldVarInfoGV->getValueType());
    std::vector<llvm::Type*> newVarInfoMemberTypes(oldVarInfoType->element_begin(), oldVarInfoType->element_end());
    newVarInfoMemberTypes.insert(newVarInfoMemberTypes.end() - 1, newVarInfoMemberType);
    llvm::StructType* newVarInfoType = llvm::StructType::get(ctx, newVarInfoMemberTypes, oldVarInfoType->isPacked());
    llvm::Constant* oldVarInfoInit = oldVarInfoGV->getInitializer();
    unsigned oldVarInfoInitMemSize = oldVarInfoInit->getType()->getStructNumElements();
    std::vector<llvm::Constant*> newVarInfoInitMembers;
    for (unsigned i = 0; i < oldVarInfoInitMemSize - 1; ++i) {
        newVarInfoInitMembers.push_back(oldVarInfoInit->getAggregateElement(i));
    }
    newVarInfoInitMembers.push_back(newVarInfoMemberInit);
    newVarInfoInitMembers.push_back(oldVarInfoInit->getAggregateElement(oldVarInfoInitMemSize - 1));
    llvm::Constant* newVarInfoInit = llvm::ConstantStruct::get(newVarInfoType, newVarInfoInitMembers);
    llvm::GlobalVariable* newVarInfoGV = new llvm::GlobalVariable(
        M,
        newVarInfoType,
        oldVarInfoGV->isConstant(),
        oldVarInfoGV->getLinkage(),
        newVarInfoInit,
        oldVarInfoName
    );
    newVarInfoGV->copyAttributesFrom(oldVarInfoGV);
    llvm::SmallVector< llvm::DIGlobalVariableExpression *, 1 > oldVarInfoDIs;
    oldVarInfoGV->getDebugInfo(oldVarInfoDIs);
    for (auto *DIE : oldVarInfoDIs) {
        newVarInfoGV->addDebugInfo(DIE);
    }
    oldVarInfoGV->replaceAllUsesWith(newVarInfoGV);
    oldVarInfoGV->eraseFromParent();
}

/**
 * 1. @start_timestamp used to store the flight control start timestamp, it's useful for the log file name.
 * 2. @rvlLogFileNameConstant defined as property_name_Rvl_%d_%ld.in, %d is the process id, %ld is the start_timestamp.
 * 3. @abMode defined as "ab", it's the mode for fopen.
 * 4. @rvlLogFilePtr is a global pointer to store the file pointer after call fopen(). It's thread local.
 * 5. @rvlLogFileName is a global char array to store the file name. It stores the file name after snprintf(@rvlLogFileNameConstant).
 * 6. check @rvlLogFilePtr, if it's null, we call snprintf(@rvlLogFileName) to setup the file name and call fopen(@rvlLogFileName, @abMode) to open the file.
 * 
 * We insert @start_timestamp initialization to the llvm.global_ctors section
 */
llvm::Function* instrumenter::Inputgetter::createGlobalInitHelper(llvm::Module &M){
    createStartTimestampInit(M);
    llvm::LLVMContext &ctx = M.getContext();
    llvm::FunctionType *voidTy = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false);
    llvm::Function *global_init_help = llvm::Function::Create(voidTy, llvm::Function::InternalLinkage, "global_init_help", &M);
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ctx, "entry", global_init_help);
    llvm::IRBuilder<> builder(entry);

    // we create declare i32 @getpid() #10 = { nounwind }
    llvm::Function* getpidFunc = M.getFunction("getpid");
    if (!getpidFunc) {
        getpidFunc = createGetpidFunc(M);
    }

    llvm::GlobalVariable* start_timestamp = M.getNamedGlobal("start_timestamp")? M.getNamedGlobal("start_timestamp") : createStartTimestamp(M);

    // we create two string constants for the file name and the mode
    llvm::GlobalVariable* rvlLogFileNameConstant = M.getNamedGlobal("rvlLogFileNameConstant")? M.getNamedGlobal("rvlLogFileNameConstant") : createStringConstant(M, _curr_property + "_Rvl_%d_%ld.in", "rvlLogFileNameConstant");
    llvm::GlobalVariable* abMode = M.getNamedGlobal("abMode")? M.getNamedGlobal("abMode") : createStringConstant(M, "ab", "abMode");

    // we create a file pointer
    // @rvlLogFilePtr = internal thread_local global ptr zeroinitializer, align 8
    llvm::GlobalVariable* rvlLogFilePtr = M.getNamedGlobal("rvlLogFilePtr");
    if (!rvlLogFilePtr) {
        rvlLogFilePtr = createGlobalPointer(M, "rvlLogFilePtr");
        rvlLogFilePtr->setThreadLocalMode(llvm::GlobalValue::ThreadLocalMode::GeneralDynamicTLSModel);
    }

    // create @file_name = dso_local thread_local global [50 x i8] zeroinitializer, align 1
    llvm::GlobalVariable* rvlLogFileName = M.getNamedGlobal("rvlLogFileName");
    if (!rvlLogFileName) {
        rvlLogFileName = createGlobalCStrting(M, "rvlLogFileName", 50);
        rvlLogFileName->setThreadLocalMode(llvm::GlobalValue::ThreadLocalMode::GeneralDynamicTLSModel);
    }

    // we check if the file is already opened
    // threadLocalrvlLogFilePtr = call align 8 ptr @llvm.threadlocal.address(ptr align 8 rvlLogFilePtr)
    // rvlLogFilePtrLoad = load ptr, ptr threadLocalrvlLogFilePtr
    // isNull = icmp eq ptr rvlLogFilePtrLoad, null
    // br i1 isNull, label %end, label %Null
    llvm::CallInst* threadLocalrvlLogFilePtr = builder.CreateThreadLocalAddress(rvlLogFilePtr);
    llvm::LoadInst* rvlLogFilePtrLoad = builder.CreateLoad(rvlLogFilePtr->getType(), threadLocalrvlLogFilePtr, "loadRvlLogFilePtr");
    llvm::Value* isNull = builder.CreateICmpEQ(rvlLogFilePtrLoad, llvm::ConstantPointerNull::get(llvm::PointerType::get(llvm::Type::getInt8PtrTy(M.getContext()), 0)), "isNull");
    llvm::BasicBlock* Null = llvm::BasicBlock::Create(ctx, "Null", global_init_help);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(ctx, "end", global_init_help);
    builder.CreateCondBr(isNull, Null, end);
    // Null:
    builder.SetInsertPoint(Null);
    // we insert the fllowing code to setup file name
    // rvlLogFileNameThreadAddr = call align 1 ptr @llvm.threadlocal.address.p0(ptr align 1 @rvlLogFileName)
    // gep = getelementptr inbounds @rvlLogFileName->getType(), ptr rvlLogFileNameThreadAddr, i64 0, i64 0
    // pid = call i32 @getpid()
    // start_timestamp = load i64, ptr @start_timestamp, align 8
    // %9 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr noundef rvlLogFileNameThreadAddr, i64 noundef 50, ptr noundef rvlLogFileNameConstant, i32 noundef pid, i64 noundef start_timestamp) #10 attributes #10 = { nounwind }
    llvm::ArrayType* arrayType = llvm::cast<llvm::ArrayType>(rvlLogFileName->getValueType());
    llvm::CallInst* rvlLogFileNameThreadAddr = builder.CreateThreadLocalAddress(rvlLogFileName);
    llvm::Value* gep = builder.CreateGEP(arrayType, rvlLogFileNameThreadAddr, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), 0)});
    llvm::CallInst* pid = builder.CreateCall(getpidFunc);
    pid->setName("pid");
    llvm::LoadInst* loadInst = builder.CreateLoad(llvm::Type::getInt64Ty(M.getContext()), start_timestamp, "start_timestamp");
    llvm::Function* snprintfFunc = M.getFunction("snprintf");
    if (!snprintfFunc) {
        snprintfFunc = createSnprintf(M);
    }
    llvm::CallInst* snprintfCall = builder.CreateCall(snprintfFunc, {gep, llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), arrayType->getNumElements()), rvlLogFileNameConstant, pid, loadInst}, "snprintfCall");
    snprintfCall->addFnAttr(llvm::Attribute::NoUnwind);

    // next we call fopen() to open the file
    // %12 = call noalias ptr @fopen(ptr noundef gep, ptr noundef abMode)
    // %7 = call align 8 ptr @llvm.threadlocal.address(ptr align 8 rvlLogFilePtr)
    // store ptr %12, ptr %7, align 8
    llvm::Function* fopenFunc = M.getFunction("fopen");
    if (!fopenFunc) {
        fopenFunc = createFopenFunc(M);
    }
    llvm::CallInst* fopenCall = builder.CreateCall(fopenFunc, {gep, abMode}, "");
    llvm::CallInst* threadLocalAddr = builder.CreateThreadLocalAddress(rvlLogFilePtr);
    llvm::StoreInst* storeInst = builder.CreateStore(fopenCall, threadLocalAddr);
    builder.CreateBr(end);
    // end:
    builder.SetInsertPoint(end);
    // create ret void
    builder.CreateRetVoid();
    global_init_help->addFnAttr(llvm::Attribute::NoInline);
    global_init_help->addFnAttr(llvm::Attribute::NoUnwind);
    // global_init_help->addFnAttr(llvm::Attribute::UWTable);
    global_init_help->addFnAttr(llvm::Attribute::get(ctx, "frame-pointer", "non-leaf"));
    global_init_help->addFnAttr(llvm::Attribute::get(ctx, "no-trapping-math", "true"));
    global_init_help->addFnAttr(llvm::Attribute::get(ctx, "stack-protector-buffer-size", "8"));
    global_init_help->addFnAttr(llvm::Attribute::get(ctx, "target-cpu", "generic"));
    global_init_help->addFnAttr(llvm::Attribute::get(ctx, "target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"));
    return global_init_help;
}

// we create retValCollector(i32 noundef %id, ptr noundef %RetAlloca, i64 noundef %size) #1 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
// COMMENTED: Function return value collection feature is disabled
/*
llvm::Function* instrumenter::Inputgetter::createRetValCollector(llvm::Module &M) {
    llvm::LLVMContext &context = M.getContext();
    llvm::Type* i32 = llvm::Type::getInt32Ty(context);
    llvm::Type* i64 = llvm::Type::getInt64Ty(context);
    llvm::Type* i8Ptr = llvm::Type::getInt8PtrTy(context);
    llvm::FunctionType* retValCollectorType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i32, i8Ptr, i64}, false);
    llvm::Function* retValCollector = llvm::Function::Create(retValCollectorType, llvm::Function::ExternalLinkage, "retValCollector", &M);
    
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", retValCollector);
    llvm::IRBuilder<> builder(entry);
    
    llvm::GlobalVariable* counter = M.getNamedGlobal("counter");
    assert(counter && "global variable counter is not exist.");
    llvm::LoadInst* loadCounter = builder.CreateLoad(counter->getValueType(), counter, "loadCounter");
    llvm::Value* isCounterZero = builder.CreateICmpEQ(loadCounter, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "isCounterZero");
    
    llvm::BasicBlock* counterOneBlock = llvm::BasicBlock::Create(context, "counterOne", retValCollector);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(context, "end", retValCollector);
    
    builder.CreateCondBr(isCounterZero, counterOneBlock, end);
    
    // counterOneBlock:
    builder.SetInsertPoint(counterOneBlock);
    
    // uid_concat_size = alloca i64
    // %uid.ext = zext i32 %uid to i64
    // %size.ext = zext i64 %size to i64
    // %concat = shl i64 %uid.ext, 32
    // %concat = or i64 %concat, %size.ext
    // store %concat, ptr uid_size
    llvm::AllocaInst* uid_size = builder.CreateAlloca(i64, nullptr, "uid_size");
    llvm::Value* uid = builder.CreateZExt(retValCollector->arg_begin(), i64);
    auto arg_it = retValCollector->arg_begin();
    std::advance(arg_it, 2);
    llvm::Value* size = &*arg_it;
    llvm::Value* uid_ext = builder.CreateShl(uid, 32);
    llvm::Value* concat = builder.CreateOr(uid_ext, size);
    llvm::StoreInst* storeFirst = builder.CreateStore(concat, uid_size);
    
    // threadLocalAddr = call align 8 ptr @llvm.threadlocal.address(ptr align 8 rvlLogFilePtr)
    llvm::GlobalVariable* rvlLogFilePtr;
    if (M.getNamedGlobal("rvlLogFilePtr")) {
        rvlLogFilePtr = M.getNamedGlobal("rvlLogFilePtr");
    } else {
        rvlLogFilePtr = createGlobalPointer(M, "rvlLogFilePtr");
        rvlLogFilePtr->setThreadLocalMode(llvm::GlobalValue::ThreadLocalMode::GeneralDynamicTLSModel);
    }

    llvm::CallInst* threadLocalAddr = builder.CreateThreadLocalAddress(rvlLogFilePtr);
    
    // rvlLogFilePtrLoad1 = load ptr, ptr threadLocalAddr
    llvm::LoadInst* rvlLogFilePtrLoad1 = builder.CreateLoad(rvlLogFilePtr->getType(), threadLocalAddr, "loadRvlLogFilePtr");
    
    // isNull = icmp eq ptr rvlLogFilePtrLoad1, null
    llvm::Value* isNull = builder.CreateICmpEQ(rvlLogFilePtrLoad1, llvm::ConstantPointerNull::get(llvm::PointerType::get(i8Ptr, 0)), "isNull");
    
    // br i1 isNull, label %end, label %notNull
    llvm::BasicBlock* notNull = llvm::BasicBlock::Create(context, "notNull", retValCollector);
    
    builder.CreateCondBr(isNull, end, notNull);
    
    // notNull:
    builder.SetInsertPoint(notNull);
    
    // fwriteID = call i64 @fwrite(ptr noundef uid+size, i64 noundef 8, i64 noundef 1, ptr noundef rvlLogFilePtrLoad1)
    llvm::Function* fwriteFunc = M.getFunction("fwrite");
    if (!fwriteFunc) {
        fwriteFunc = createFwriteFunc(M);
    }
    llvm::CallInst* fwriteID = builder.CreateCall(fwriteFunc, {uid_size, llvm::ConstantInt::get(i64, 8), llvm::ConstantInt::get(i64, 1), rvlLogFilePtrLoad1}, "fwriteID");
    
    // rvlLogFilePtrLoad2 = load ptr, ptr threadLocalAddr
    // fwriteRet = call i64 @fwrite(ptr noundef retVal, i64 noundef size, i64 noundef 1, ptr noundef rvlLogFilePtrLoad2)
    llvm::LoadInst* rvlLogFilePtrLoad2 = builder.CreateLoad(rvlLogFilePtr->getType(), threadLocalAddr, "loadRvlLogFilePtr");
    llvm::CallInst* fwriteRetVal = builder.CreateCall(fwriteFunc, {retValCollector->arg_begin()+1, retValCollector->arg_begin()+2, llvm::ConstantInt::get(i64, 1), rvlLogFilePtrLoad2}, "fwriteRetVal");
    
    // br label %end
    builder.CreateBr(end);
    
    // end:
    builder.SetInsertPoint(end);
    builder.CreateRetVoid();

    // add attributes for args
    for (unsigned i = 0; i < retValCollector->arg_size(); ++i) {
        retValCollector->addParamAttr(i, llvm::Attribute::AttrKind::NoUndef);
    }
    // add attributes
    retValCollector->addFnAttr(llvm::Attribute::MustProgress);
    retValCollector->addFnAttr(llvm::Attribute::NoInline);
    retValCollector->addFnAttr(llvm::Attribute::NoUnwind);
    retValCollector->addFnAttr(llvm::Attribute::OptimizeNone);
    retValCollector->addFnAttr(llvm::Attribute::UWTable);
    retValCollector->addFnAttr("frame-pointer", "non-leaf");
    retValCollector->addFnAttr("no-trapping-math", "true");
    retValCollector->addFnAttr("stack-protector-buffer-size", "8");
    retValCollector->addFnAttr("target-cpu", "generic");
    retValCollector->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    return retValCollector;
}
*/  // End of commented createRetValCollector

/**
 * we init the start_timestamp global variable at the beginning of the program (append into GlobalCtors). 
 */
llvm::Function* instrumenter::Inputgetter::createStartTimestampInit(llvm::Module &M) {
    llvm::LLVMContext &ctx = M.getContext();
    llvm::FunctionType *voidTy = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false);
    llvm::Function *start_timestamp_init = llvm::Function::Create(voidTy, llvm::Function::InternalLinkage, "start_timestamp_init", &M);
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ctx, "entry", start_timestamp_init);
    llvm::IRBuilder<> builder(entry);
    // @start_timestamp = dso_local global i64 0, align 8
    llvm::GlobalVariable* start_timestamp = M.getNamedGlobal("start_timestamp");
    if (!start_timestamp) {
        start_timestamp = createStartTimestamp(M);
    }
    // we first insert %9 = call i64 @time(ptr noundef @start_timestamp) #10 attributes #10 = { nounwind } into the main function beginning
    llvm::Function* timeFunc = M.getFunction("time");
    if (!timeFunc) {
        timeFunc = createTimeFunc(M);
    }
    llvm::CallInst* timeCall = builder.CreateCall(timeFunc, {start_timestamp}, "");
    timeCall->addFnAttr(llvm::Attribute::NoUnwind);
    builder.CreateRetVoid();
    //attributes #0 = { noinline nounwind uwtable "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
    start_timestamp_init->addFnAttr(llvm::Attribute::NoInline);
    start_timestamp_init->addFnAttr(llvm::Attribute::NoUnwind);
    // start_timestamp_init->addFnAttr(llvm::Attribute::UWTable);
    start_timestamp_init->addFnAttr(llvm::Attribute::get(ctx, "frame-pointer", "non-leaf"));
    start_timestamp_init->addFnAttr(llvm::Attribute::get(ctx, "no-trapping-math", "true"));
    start_timestamp_init->addFnAttr(llvm::Attribute::get(ctx, "stack-protector-buffer-size", "8"));
    start_timestamp_init->addFnAttr(llvm::Attribute::get(ctx, "target-cpu", "generic"));
    start_timestamp_init->addFnAttr(llvm::Attribute::get(ctx, "target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"));
    start_timestamp_init->setSection(".text.startup");
    llvm::appendToGlobalCtors(M, start_timestamp_init, 65534);
    return start_timestamp_init;
}

llvm::Function* instrumenter::Inputgetter::createDumpEntryArgsFunc(llvm::Module &M, std::vector<llvm::Type*> entryArgTypes) {
    llvm::LLVMContext &ctx = M.getContext();
    llvm::Function* snprintfFunc = M.getFunction("snprintf");
    if (!snprintfFunc) {
        snprintfFunc = createSnprintf(M);
    }
    llvm::Function* fopenFunc = M.getFunction("fopen");
    if (!fopenFunc) {
        fopenFunc = createFopenFunc(M);
    }
    llvm::Function* fwriteFunc = M.getFunction("fwrite");
    if (!fwriteFunc) {
        fwriteFunc = createFwriteFunc(M);
    }
    llvm::Function* fflushFunc = M.getFunction("fflush");
    if (!fflushFunc) {
        fflushFunc = createFflushFunc(M);
    }
    llvm::Function* fcloseFunc = M.getFunction("fclose");
    if (!fcloseFunc) {
        fcloseFunc = createFcloseFunc(M);
    }
    // create dump_entry_args(...) function
    // declare void @dump_entry_args(...) #1
    // #1 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
    llvm::GlobalVariable* entryArgsFileName = M.getNamedGlobal("entryArgsFileName") ? M.getNamedGlobal("entryArgsFileName") : createStringConstant(M, _curr_property + "_EntryArgs_%ld.in", "entryArgsFileName");
    llvm::GlobalVariable* curr_timestamp = M.getNamedGlobal("curr_timestamp") ? M.getNamedGlobal("curr_timestamp") : createCurrTimestamp(M);
    // set linkage to external
    entryArgsFileName->setLinkage(llvm::GlobalValue::ExternalLinkage);
    std::vector<llvm::Type*> paramTypes(entryArgTypes.begin(), entryArgTypes.end());
    llvm::FunctionType* voidFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), paramTypes, false);
    llvm::Function* dumpEntryArgsFunc = llvm::Function::Create(voidFuncType, llvm::Function::ExternalLinkage, "dump_entry_args", &M);
    // create entry block
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", dumpEntryArgsFunc);
    llvm::IRBuilder<> builder(entry);
    llvm::ArrayType* arrayType = llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 100);
    llvm::AllocaInst* entryArgsFileNameStr = builder.CreateAlloca(arrayType, nullptr, "entryArgsFileNameStr");
    llvm::AllocaInst* entryArgsFileNamePtr = builder.CreateAlloca(llvm::Type::getInt8PtrTy(ctx), nullptr, "entryArgsFileNamePtr");
    llvm::Value* getFileNameStrAddr = builder.CreateGEP(arrayType, entryArgsFileNameStr, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::Value* gep1 = builder.CreateGEP(arrayType, getFileNameStrAddr, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::LoadInst* loadCurrentTime = builder.CreateLoad(llvm::Type::getInt64Ty(ctx), curr_timestamp, "loadCurrentTime");
    llvm::GlobalVariable* wbMode = M.getNamedGlobal("wbMode")? M.getNamedGlobal("wbMode") : createStringConstant(M, "wb", "wbMode");
    llvm::CallInst* callSnprintf = builder.CreateCall(snprintfFunc, {gep1, llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 100), entryArgsFileName, loadCurrentTime}, "concatEntryArgsFileName");
    llvm::Value* gep2 = builder.CreateGEP(arrayType, entryArgsFileNameStr, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::Value* gep3 = builder.CreateGEP(arrayType, gep2, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::CallInst* callFopen = builder.CreateCall(fopenFunc, {gep3, wbMode}, "");
    llvm::StoreInst* storeFopen = builder.CreateStore(callFopen, entryArgsFileNamePtr);
    llvm::LoadInst* loadFopen = builder.CreateLoad(entryArgsFileNamePtr->getType(), entryArgsFileNamePtr, "");
    // we write each argument to the file
    // format: 1 byte id + 3 bytes size + arg value
    // 1. write id + size
    // 2. write arg value
    for (auto arg = dumpEntryArgsFunc->arg_begin(); arg != dumpEntryArgsFunc->arg_end(); ++arg) {
        uint32_t argIndex = std::distance(dumpEntryArgsFunc->arg_begin(), arg);
        llvm::Type* argType = entryArgTypes[argIndex];
        uint32_t typeSize = M.getDataLayout().getTypeAllocSize(argType);
        
        // Create a 4-byte header: 1 byte id + 3 bytes size
        // Combine them into a single 32-bit value: (id << 24) | (size & 0x00FFFFFF)
        uint32_t header = (argIndex << 24) | (typeSize & 0x00FFFFFF);
        
        // Allocate space for the header
        llvm::AllocaInst* headerAlloca = builder.CreateAlloca(llvm::Type::getInt32Ty(ctx), nullptr, "argHeader");
        builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), header), headerAlloca);
        
        llvm::Type *i64 = llvm::Type::getInt64Ty(M.getContext());
        llvm::Constant* headerSize = llvm::ConstantInt::get(i64, 4);
        llvm::Constant* argSize = llvm::ConstantInt::get(i64, typeSize);
        llvm::Constant* one = llvm::ConstantInt::get(i64, 1);
        
        // 1. Write id + size (header)
        llvm::CallInst* callFwriteHeader = builder.CreateCall(fwriteFunc, {headerAlloca, headerSize, one, loadFopen}, "fwriteHeader");
        for (unsigned i = 0; i < callFwriteHeader->arg_size(); ++i) {
            callFwriteHeader->addParamAttr(i, llvm::Attribute::NoUndef);
        }
        
        // 2. Write arg value (store locally to get a stable pointer for fwrite)
        llvm::AllocaInst* argValueAlloca = builder.CreateAlloca(argType, nullptr, "argValue");
        builder.CreateStore(&*arg, argValueAlloca);
        llvm::CallInst* callFwrite = builder.CreateCall(fwriteFunc, {argValueAlloca, argSize, one, loadFopen}, "fwriteArg");
        for (unsigned i = 0; i < callFwrite->arg_size(); ++i) {
            callFwrite->addParamAttr(i, llvm::Attribute::NoUndef);
        }
    }
    llvm::CallInst* callFflush = builder.CreateCall(fflushFunc, {loadFopen}, "");
    llvm::CallInst* callFclose = builder.CreateCall(fcloseFunc, {loadFopen}, "");
    builder.CreateRetVoid();
    // add attributes #1
    dumpEntryArgsFunc->addFnAttr(llvm::Attribute::MustProgress);
    dumpEntryArgsFunc->addFnAttr(llvm::Attribute::NoInline);
    dumpEntryArgsFunc->addFnAttr(llvm::Attribute::NoUnwind);
    dumpEntryArgsFunc->addFnAttr(llvm::Attribute::OptimizeNone);
    // dumpEntryArgsFunc->addFnAttr(llvm::Attribute::UWTable);
    dumpEntryArgsFunc->addFnAttr(llvm::Attribute::get(ctx, "frame-pointer", "non-leaf"));
    dumpEntryArgsFunc->addFnAttr(llvm::Attribute::get(ctx, "no-trapping-math", "true"));
    dumpEntryArgsFunc->addFnAttr(llvm::Attribute::get(ctx, "stack-protector-buffer-size", "8"));
    dumpEntryArgsFunc->addFnAttr(llvm::Attribute::get(ctx, "target-cpu", "generic"));
    dumpEntryArgsFunc->addFnAttr(llvm::Attribute::get(ctx, "target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"));
    return dumpEntryArgsFunc;
}

llvm::Function* instrumenter::Inputgetter::createDumpHeapVarsFunc(llvm::Module &M){
    llvm::LLVMContext &ctx = M.getContext();
    // create dump_heap_vars() function
    // declare void @dump_heap_vars() #1
    // #1 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
    llvm::GlobalVariable* heapLogFileName = M.getNamedGlobal("heapLogFileName") ? M.getNamedGlobal("heapLogFileName") : createStringConstant(M, _curr_property + "_Heap_%ld.in", "heapLogFileName");
    // set linkage to external
    heapLogFileName->setLinkage(llvm::GlobalValue::ExternalLinkage);
    llvm::FunctionType* voidFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false);
    llvm::Function* dumpHeapVarsFunc = llvm::Function::Create(voidFuncType, llvm::Function::ExternalLinkage, "dump_heap_vars", &M);
    // add attributes #1
    dumpHeapVarsFunc->addFnAttr(llvm::Attribute::MustProgress);
    dumpHeapVarsFunc->addFnAttr(llvm::Attribute::NoInline);
    dumpHeapVarsFunc->addFnAttr(llvm::Attribute::NoUnwind);
    dumpHeapVarsFunc->addFnAttr(llvm::Attribute::OptimizeNone);
    // dumpHeapVarsFunc->addFnAttr(llvm::Attribute::UWTable);
    dumpHeapVarsFunc->addFnAttr(llvm::Attribute::get(ctx, "frame-pointer", "non-leaf"));
    dumpHeapVarsFunc->addFnAttr(llvm::Attribute::get(ctx, "no-trapping-math", "true"));
    dumpHeapVarsFunc->addFnAttr(llvm::Attribute::get(ctx, "stack-protector-buffer-size", "8"));
    dumpHeapVarsFunc->addFnAttr(llvm::Attribute::get(ctx, "target-cpu", "generic"));
    dumpHeapVarsFunc->addFnAttr(llvm::Attribute::get(ctx, "target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"));

    return dumpHeapVarsFunc;
}

void instrumenter::Inputgetter::warpDynamicMemoryAllocation(llvm::Module &M) {
    llvm::LLVMContext &ctx = M.getContext();
    
    // Create all hook functions
    llvm::Type* voidPtrTy = llvm::Type::getInt8PtrTy(ctx);
    llvm::Type* sizeTy = llvm::Type::getInt64Ty(ctx);
    
    // malloc_hook(size_t size, size_t typeHash)
    llvm::FunctionType* mallocHookFuncType = llvm::FunctionType::get(voidPtrTy, {sizeTy, sizeTy}, false);
    llvm::Function* mallocHookFunc = llvm::Function::Create(mallocHookFuncType, llvm::Function::ExternalLinkage, "malloc_hook", &M);
    
    // calloc_hook(size_t num, size_t size, size_t typeHash)
    llvm::FunctionType* callocHookFuncType = llvm::FunctionType::get(voidPtrTy, {sizeTy, sizeTy, sizeTy}, false);
    llvm::Function* callocHookFunc = llvm::Function::Create(callocHookFuncType, llvm::Function::ExternalLinkage, "calloc_hook", &M);
    
    // realloc_hook(void* ptr, size_t size, size_t typeHash)
    llvm::FunctionType* reallocHookFuncType = llvm::FunctionType::get(voidPtrTy, {voidPtrTy, sizeTy, sizeTy}, false);
    llvm::Function* reallocHookFunc = llvm::Function::Create(reallocHookFuncType, llvm::Function::ExternalLinkage, "realloc_hook", &M);
    
    // free_hook(void* ptr)
    llvm::FunctionType* freeHookFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {voidPtrTy}, false);
    llvm::Function* freeHookFunc = llvm::Function::Create(freeHookFuncType, llvm::Function::ExternalLinkage, "free_hook", &M);
    
    // new_hook(size_t size, size_t typeHash)
    llvm::FunctionType* newHookFuncType = llvm::FunctionType::get(voidPtrTy, {sizeTy, sizeTy}, false);
    llvm::Function* newHookFunc = llvm::Function::Create(newHookFuncType, llvm::Function::ExternalLinkage, "new_hook", &M);
    
    // new_array_hook(size_t size, size_t typeHash, size_t cookie)
    llvm::FunctionType* newArrayHookFuncType = llvm::FunctionType::get(voidPtrTy, {sizeTy, sizeTy, sizeTy}, false);
    llvm::Function* newArrayHookFunc = llvm::Function::Create(newArrayHookFuncType, llvm::Function::ExternalLinkage, "new_array_hook", &M);
    
    // delete_hook(void* ptr)
    llvm::FunctionType* deleteHookFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {voidPtrTy}, false);
    llvm::Function* deleteHookFunc = llvm::Function::Create(deleteHookFuncType, llvm::Function::ExternalLinkage, "delete_hook", &M);
    
    // delete_array_hook(void* ptr)
    llvm::FunctionType* deleteArrayHookFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {voidPtrTy}, false);
    llvm::Function* deleteArrayHookFunc = llvm::Function::Create(deleteArrayHookFuncType, llvm::Function::ExternalLinkage, "delete_array_hook", &M);
    
    // Get typeHash from metadata !heapAllocType, such as '!239889 = !{!"class.CompassCalibrator", i1 true, i64 6895277336389924950}'. The typeHash is the last element.
    auto getTypeHash = [&ctx](llvm::CallBase* CI) -> llvm::Value* {
        llvm::MDNode* heapAllocTypeMD = CI->getMetadata("heapAllocType");
        if (heapAllocTypeMD && heapAllocTypeMD->getNumOperands() > 0) {
            // We need to get the last element of the metadata. It's the typeHash.
            unsigned lastIdx = heapAllocTypeMD->getNumOperands() - 1;
            auto* constMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(heapAllocTypeMD->getOperand(lastIdx));
            if (constMD) {
                auto* typeHashConst = llvm::dyn_cast<llvm::ConstantInt>(constMD->getValue());
                if (typeHashConst) {
                    return typeHashConst;
                }
            }
        }
        // If metadata is not found, return 0.
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
    };
    
    // Create a lambda to replace a call with a hook function call.
    auto replaceCall = [](llvm::CallBase* CI, llvm::Function* hookFunc, llvm::ArrayRef<llvm::Value*> args) {
        llvm::IRBuilder<> builder(CI);
        llvm::CallInst* newCall = builder.CreateCall(hookFunc, args);
        
        // Copy metadata and attributes from the original call to the new call.
        if (CI->hasMetadata()) {
            llvm::SmallVector<std::pair<unsigned, llvm::MDNode*>, 4> MDs;
            if (auto* Inst = llvm::dyn_cast<llvm::Instruction>(CI)) {
                Inst->getAllMetadata(MDs);
                for (const auto& MD : MDs) {
                    newCall->setMetadata(MD.first, MD.second);
                }
            }
        }
        newCall->setCallingConv(CI->getCallingConv());
        newCall->setAttributes(CI->getAttributes());
        
        // If the original call has a return value, replace all uses of the original call with the new call.
        if (!CI->getType()->isVoidTy()) {
            CI->replaceAllUsesWith(newCall);
        }
        
        // Delete the original call.
        CI->eraseFromParent();
    };
    
    // Collect all calls to malloc, calloc, realloc, free, new, new[], delete, and delete[].
    std::vector<llvm::CallBase*> callsToReplace;
    
    for (auto &F : M) {
        // We need careful handling new and delete opraotors, when then are overloaded, they are not declaration.
        if (F.isDeclaration() || F.getName() == "_Znwm" || F.getName() == "_Znam" || F.getName() == "_ZdlPv" || F.getName() == "_ZdaPv") {
            continue;
        }
        
        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *CI = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    if (!CI->isIndirectCall()) {
                        llvm::Function* calledFunc = CI->getCalledFunction();
                        if (calledFunc) {
                            std::string funcName = calledFunc->getName().str();
                            if (funcName == "malloc" || funcName == "calloc" || funcName == "realloc" || 
                                funcName == "free" || funcName == "_Znwm" || funcName == "_Znam" || 
                                funcName == "_ZdlPv" || funcName == "_ZdaPv") {
                                callsToReplace.push_back(CI);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Replace all target calls with hook function calls.
    for (auto *CI : callsToReplace) {
        llvm::Function* calledFunc = CI->getCalledFunction();
        std::string funcName = calledFunc->getName().str();
        
        if (funcName == "malloc") {
            // malloc_hook(size, typeHash)
            llvm::Value* sizeArg = CI->getArgOperand(0);
            llvm::Value* typeHashArg = getTypeHash(CI);
            replaceCall(CI, mallocHookFunc, {sizeArg, typeHashArg});
        }
        else if (funcName == "calloc") {
            // calloc_hook(num, size, typeHash)
            llvm::Value* numArg = CI->getArgOperand(0);
            llvm::Value* sizeArg = CI->getArgOperand(1);
            llvm::Value* typeHashArg = getTypeHash(CI);
            replaceCall(CI, callocHookFunc, {numArg, sizeArg, typeHashArg});
        }
        else if (funcName == "realloc") {
            // realloc_hook(ptr, size, typeHash)
            llvm::Value* ptrArg = CI->getArgOperand(0);
            llvm::Value* sizeArg = CI->getArgOperand(1);
            llvm::Value* typeHashArg = getTypeHash(CI);
            replaceCall(CI, reallocHookFunc, {ptrArg, sizeArg, typeHashArg});
        }
        else if (funcName == "free") {
            // free_hook(ptr)
            llvm::Value* ptrArg = CI->getArgOperand(0);
            replaceCall(CI, freeHookFunc, {ptrArg});
        }
        else if (funcName == "_Znwm") {
            // new_hook(size, typeHash)
            llvm::Value* sizeArg = CI->getArgOperand(0);
            llvm::Value* typeHashArg = getTypeHash(CI);
            replaceCall(CI, newHookFunc, {sizeArg, typeHashArg});
        }
        else if (funcName == "_Znam") {
            // new_array_hook(size, typeHash, cookie)
            llvm::Value* sizeArg = CI->getArgOperand(0);
            llvm::Value* typeHashArg = getTypeHash(CI);
            // Get cookie from metadata !newOpCookie, such as '!239890 = !{i64 8}'. The cookie is the first element.
            llvm::MDNode* newOpCookieMD = CI->getMetadata("newOpCookie");
            llvm::Value* cookieArg = nullptr;
            if (newOpCookieMD && newOpCookieMD->getNumOperands() > 0) {
                auto* constMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(newOpCookieMD->getOperand(0));
                if (constMD) {
                    auto* cookieConst = llvm::dyn_cast<llvm::ConstantInt>(constMD->getValue());
                    if (cookieConst) {
                        cookieArg = cookieConst;
                    }
                }
            }
            if (!cookieArg) {
                cookieArg = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
            }
            replaceCall(CI, newArrayHookFunc, {sizeArg, typeHashArg, cookieArg});
        }
        else if (funcName == "_ZdlPv") {
            // delete_hook(ptr)
            llvm::Value* ptrArg = CI->getArgOperand(0);
            replaceCall(CI, deleteHookFunc, {ptrArg});
        }
        else if (funcName == "_ZdaPv") {
            // delete_array_hook(ptr)
            llvm::Value* ptrArg = CI->getArgOperand(0);
            replaceCall(CI, deleteArrayHookFunc, {ptrArg});
        }
    }
}

llvm::StringRef instrumenter::Inputgetter::getStructTypeNamePrefix(llvm::StringRef Name) {
    while (!Name.empty()) {
      size_t DotPos = Name.rfind('.');
      if (DotPos == llvm::StringRef::npos || DotPos == 0 || DotPos == Name.size() - 1)
        return Name;
        
      // 检查点号后面是否都是数字
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

size_t instrumenter::Inputgetter::hashType(llvm::Type* Ty) {
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
            llvm::StringRef varArg = FTy->isVarArg() ? "varArg:true" : "varArg:false";
            return llvm::stable_hash_combine(retHash, stable_hash_combine_string(varArg));
          }
        case llvm::Type::StructTyID: {
            llvm::StructType *STy = cast<llvm::StructType>(Ty);
        
            if (!STy->isLiteral()) {
                size_t nameHash = stable_hash_combine_string(getStructTypeNamePrefix(STy->getName()));
                llvm::StringRef packed = STy->isPacked() ? "packed:true" : "packed:false";
                size_t retHash = stable_hash_combine_string(packed);
                for (llvm::Type *elemTy : STy->elements()) {
                    nameHash = llvm::stable_hash_combine(nameHash, hashType(elemTy));
                }
                return llvm::stable_hash_combine(nameHash, retHash);
            }else{
                llvm::StringRef packed = STy->isPacked() ? "packed:true" : "packed:false";
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

void instrumenter::Inputgetter::supplementDMAType(llvm::Module &M) {
    // Iterate through all callsites to find which with metadata !heapAllocTypeAdd
    std::vector<llvm::CallBase*> callsitesWithHeapAllocTypeAdd;
    for (auto &F : M) {
        if (F.isDeclaration()) continue; // Skip declarations.
        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *CI = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    llvm::MDNode* heapAllocTypeAddMD = CI->getMetadata("heapAllocTypeAdd");
                    if (heapAllocTypeAddMD) {
                        // We found a callsite with metadata !heapAllocTypeAdd.
                        // Save it for further processing.
                        callsitesWithHeapAllocTypeAdd.push_back(CI);
                    }
                }
            }
        }
    }

    std::map<llvm::Function*, llvm::Function*> oldF_to_newF; // Save the new functions created.
    std::set<std::string> dmaHookFunctions = {"malloc_hook", "calloc_hook", "realloc_hook", "new_hook", "new_array_hook"};
    
    // Helper function to recursively create hook functions for call chains
    std::function<llvm::Function*(llvm::Function*)> createHookFunctionRecursively = [&](llvm::Function* func) -> llvm::Function* {
        // If we already created a hook for this function, return it
        if (oldF_to_newF.find(func) != oldF_to_newF.end()) {
            return oldF_to_newF[func];
        }
        
        // If this is already a DMA hook function, return it as is
        if (dmaHookFunctions.find(func->getName().str()) != dmaHookFunctions.end()) {
            return func;
        }
        
        // Create a new function with "_hook" suffix and additional typeHash parameter
        llvm::ValueToValueMapTy VMap;
        llvm::FunctionType* oldFuncType = func->getFunctionType();
        std::vector<llvm::Type*> newArgTypes(oldFuncType->param_begin(), oldFuncType->param_end());
        newArgTypes.push_back(llvm::Type::getInt64Ty(M.getContext()));
        llvm::FunctionType* newFuncType = llvm::FunctionType::get(
            oldFuncType->getReturnType(), newArgTypes, oldFuncType->isVarArg());

        // Create a new function with the new function type
        llvm::Function* newF = llvm::Function::Create(
            newFuncType, 
            func->getLinkage(), 
            func->getName() + "_hook", 
            &M
        );
        
        // Set the attributes of the new function to match the old function
        newF->copyAttributesFrom(func);   

        // Create args mapping from old function to new function
        auto oldArgIter = func->arg_begin();
        auto newArgIter = newF->arg_begin();
        for (; oldArgIter != func->arg_end(); ++oldArgIter, ++newArgIter) {
            newArgIter->setName(oldArgIter->getName());
            VMap[&*oldArgIter] = &*newArgIter;
        }
        newArgIter->setName("typeHash");

        // Clone the old function to the new function
        llvm::SmallVector<llvm::ReturnInst*, 8> returns;
        llvm::CloneFunctionInto(newF, func, VMap, llvm::CloneFunctionChangeType::LocalChangesOnly, returns);

        // Save the mapping
        oldF_to_newF[func] = newF;

        // Now process all callsites in the new function
        std::vector<llvm::CallBase*> callsToReplace;
        for (auto &BB : *newF) {
            for (auto &I : BB) {
                if (auto *CI = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    if (!CI->isIndirectCall() && CI->getCalledFunction()) {
                        llvm::Function* calledFunc = CI->getCalledFunction();
                        // Check if this callsite has !DMATypeArg metadata
                        if (CI->getMetadata("DMATypeArg")) {
                            callsToReplace.push_back(CI);
                        }
                    }
                }
            }
        }

        // Replace calls that have !DMATypeArg metadata
        for (auto *CI : callsToReplace) {
            llvm::Function* calledFunc = CI->getCalledFunction();
            
            // Recursively create hook function for the callee
            llvm::Function* hookCallee = createHookFunctionRecursively(calledFunc);
            
            // Replace the call with a call to the hook function
            std::vector<llvm::Value*> args(CI->arg_begin(), CI->arg_end());
            llvm::Argument* typeHashArg = newF->arg_end() - 1;
            if (dmaHookFunctions.find(hookCallee->getName().str()) != dmaHookFunctions.end()) {
                // For DMA hook functions, replace the last argument with typeHashArg
                args.back() = typeHashArg;
            } else {
                // For non-DMA functions, append typeHashArg as a new argument
                args.push_back(typeHashArg);
            }
            
            llvm::IRBuilder<> builder(CI);
            llvm::CallInst* newCall = builder.CreateCall(hookCallee, args);
            newCall->setCallingConv(CI->getCallingConv());
            newCall->setAttributes(CI->getAttributes());
            
            // Copy metadata from original call
            if (CI->hasMetadata()) {
                llvm::SmallVector<std::pair<unsigned, llvm::MDNode*>, 4> MDs;
                CI->getAllMetadata(MDs);
                for (const auto& MD : MDs) {
                    newCall->setMetadata(MD.first, MD.second);
                }
            }
            
            CI->replaceAllUsesWith(newCall);
            CI->eraseFromParent();
        }

        return newF;
    };

    // Process callsites with !heapAllocTypeAdd metadata
    for (llvm::CallBase* CB : callsitesWithHeapAllocTypeAdd) {
        llvm::Function* callee = CB->getCalledFunction();
        if (!callee) {
            ERROR("Can't find function for callsite");
            continue;
        }
        
        std::string calleeName = callee->getName().str();
        if (dmaHookFunctions.find(calleeName) != dmaHookFunctions.end()) {
            // Replace callsite's last real argument with !heapAllocTypeAdd's hash
            llvm::MDNode* heapAllocTypeAddMD = CB->getMetadata("heapAllocTypeAdd");
            if (heapAllocTypeAddMD) {
                // We need to get the last element of the metadata. It's the typeHash.
                unsigned lastIdx = heapAllocTypeAddMD->getNumOperands() - 1;
                auto* constMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(heapAllocTypeAddMD->getOperand(lastIdx));
                if (constMD) {
                    auto* typeHashConst = llvm::dyn_cast<llvm::ConstantInt>(constMD->getValue());
                    if (typeHashConst) {
                        // Replace the last argument with typeHashConst
                        CB->setArgOperand(CB->arg_size() - 1, typeHashConst);
                    }
                }
            }
        } else {
            // Create hook function recursively for non-DMA functions
            llvm::Function* hookCallee = createHookFunctionRecursively(callee);
            
            // Replace the callsite with a call to the hook function
            std::vector<llvm::Value*> args(CB->arg_begin(), CB->arg_end());
            
            // Extract typeHash from metadata
            llvm::Value* typeHashValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), 0);
            llvm::MDNode* heapAllocTypeAddMD = CB->getMetadata("heapAllocTypeAdd");
            if (heapAllocTypeAddMD) {
                unsigned lastIdx = heapAllocTypeAddMD->getNumOperands() - 1;
                auto* constMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(heapAllocTypeAddMD->getOperand(lastIdx));
                if (constMD) {
                    auto* typeHashConst = llvm::dyn_cast<llvm::ConstantInt>(constMD->getValue());
                    if (typeHashConst) {
                        typeHashValue = typeHashConst;
                    }
                }
            }
            args.push_back(typeHashValue);
            
            llvm::IRBuilder<> builder(CB);
            llvm::CallInst* newCall = builder.CreateCall(hookCallee, args);
            newCall->setCallingConv(CB->getCallingConv());
            newCall->setAttributes(CB->getAttributes());
            
            // Copy metadata from original call
            if (CB->hasMetadata()) {
                llvm::SmallVector<std::pair<unsigned, llvm::MDNode*>, 4> MDs;
                CB->getAllMetadata(MDs);
                for (const auto& MD : MDs) {
                    newCall->setMetadata(MD.first, MD.second);
                }
            }
            
            CB->replaceAllUsesWith(newCall);
            CB->eraseFromParent();
        }
    }

    // The new functions have one more parameter than the old functions, so we carefully handled CallSites above.
    // Finally, we replace the remaining uses of old functions with new functions
    for (auto &[oldFunc, newFunc] : oldF_to_newF) {
        oldFunc->replaceAllUsesWith(newFunc);
        if (oldFunc->use_empty()) {
            oldFunc->eraseFromParent();
        }
    }
}