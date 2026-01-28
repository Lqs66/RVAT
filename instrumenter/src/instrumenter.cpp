#include "instrumenter.hh"

void instrumenter::runTimer(llvm::Module &M){
    llvm::PassBuilder PB;
    llvm::ModulePassManager MPM;
    llvm::ModuleAnalysisManager MAM;

    // Register analysis passes with the managers
    PB.registerModuleAnalyses(MAM);
    // BasicBlockTimer bbt(bb_to_timer_path);
    BasicBlockTimer bbt;
    MPM.addPass(std::move(bbt));
    MPM.run(M, MAM);
}

void instrumenter::runInputGetter(llvm::Module &M, std::vector<llvm::Function*>& entrypoints) {
    llvm::PassBuilder PB;
    llvm::ModulePassManager MPM;
    llvm::ModuleAnalysisManager MAM;

    // Register analysis passes with the managers
    PB.registerModuleAnalyses(MAM);
    Inputgetter ig(entrypoints);
    MPM.addPass(std::move(ig));
    MPM.run(M, MAM);
}

// create a global char array variable
llvm::GlobalVariable* instrumenter::createGlobalCStrting(llvm::Module &M, std::string str, int size){
    llvm::ArrayType* arrayType = llvm::ArrayType::get(llvm::Type::getInt8Ty(M.getContext()), size);
    llvm::GlobalVariable* globalArray = new llvm::GlobalVariable(M, arrayType, false, llvm::GlobalValue::InternalLinkage, nullptr, str);
    // globalArray->setSection("global_vars_tls");
    globalArray->setInitializer(llvm::ConstantAggregateZero::get(arrayType));
    // globalArray->setThreadLocalMode(llvm::GlobalValue::ThreadLocalMode::GeneralDynamicTLSModel);
    return globalArray;
}

// create a global string constant
llvm::GlobalVariable* instrumenter::createStringConstant(llvm::Module &M, std::string str, std::string name = "str") {
    llvm::LLVMContext &context = M.getContext();
    llvm::Constant* const_str = llvm::ConstantDataArray::getString(context, str);
    llvm::GlobalVariable* str_global = new llvm::GlobalVariable(M, const_str->getType(), true, llvm::GlobalValue::PrivateLinkage, const_str, name);
    str_global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    return str_global;
}

// create a global pointer variable
llvm::GlobalVariable* instrumenter::createGlobalPointer(llvm::Module &M, std::string str){
    llvm::PointerType* ptrType = llvm::PointerType::get(llvm::Type::getInt8Ty(M.getContext()), 0); 
    llvm::GlobalVariable* global_ptr = new llvm::GlobalVariable(M, ptrType, false, llvm::GlobalValue::InternalLinkage, nullptr, str);
    // global_ptr->setSection("global_vars_tls");
    // set zero initializer
    global_ptr->setInitializer(llvm::ConstantPointerNull::get(ptrType));
    return global_ptr;
}

// create a global variable to store start_timestamp
llvm::GlobalVariable* instrumenter::createStartTimestamp(llvm::Module &M){
    llvm::GlobalVariable* start_timestamp = new llvm::GlobalVariable(M, llvm::Type::getInt64Ty(M.getContext()), false, llvm::GlobalValue::ExternalLinkage, nullptr, "start_timestamp");
    start_timestamp->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), 0));
    return start_timestamp;
}

llvm::GlobalVariable* instrumenter::createCurrTimestamp(llvm::Module &M){
    llvm::GlobalVariable* curr_timestamp = new llvm::GlobalVariable(M, llvm::Type::getInt64Ty(M.getContext()), false, llvm::GlobalValue::ExternalLinkage, nullptr, "curr_timestamp");
    curr_timestamp->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), 0));
    return curr_timestamp;
}

llvm::GlobalVariable* instrumenter::createCounter(llvm::Module &M) {
    llvm::GlobalVariable* counter = new llvm::GlobalVariable(M, llvm::Type::getInt32Ty(M.getContext()), false, llvm::GlobalValue::ExternalLinkage, nullptr, "counter");
    counter->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt32Ty(M.getContext()), 0));
    return counter;
}

llvm::Function* instrumenter::ArduCopter::createDumpVarsFunc(llvm::Module &M, std::string paramName, llvm::Function* entrypoint) {
    llvm::LLVMContext& ctx = M.getContext();

    std::vector<llvm::Type*> entryArgTypes;
    for (auto &arg : entrypoint->args()) {
        entryArgTypes.push_back(arg.getType());
    }
    if (entryArgTypes.size() > 0) {
        // create dump_entry_args(...) function
        llvm::Function* dumpEntryArgsFunc = _inputgetter->createDumpEntryArgsFunc(M, entryArgTypes);
    }
    // create dump_heap_vars() function
    llvm::Function* dumpHeapVarsFunc = _inputgetter->createDumpHeapVarsFunc(M);
    // create dump_vars() function
    llvm::GlobalVariable* paramNameGV = M.getNamedGlobal(paramName);
    if (paramNameGV == nullptr) {
        std::cerr << "Can't find global variable " << paramName << std::endl;
        exit(1);
    }
    llvm::Type* paramType = llvm::dyn_cast<llvm::StructType>(paramNameGV->getValueType())->getElementType(0);
    llvm::Function* paramOperatorFunc = nullptr;
    llvm::Function* paramGetFunc = nullptr;
    llvm::Function* paramSetAndSaveFunc = nullptr;
    if (paramType->isIntegerTy(8)) {
        paramOperatorFunc = M.getFunction("_ZNK9AP_ParamTIaL11ap_var_type1EEcvRKaEv");
        paramGetFunc = M.getFunction("_ZNK9AP_ParamTIaL11ap_var_type1EE3getEv");
        paramSetAndSaveFunc = M.getFunction("_ZN9AP_ParamTIaL11ap_var_type1EE12set_and_saveERKa");
    } else if (paramType->isIntegerTy(16)) {
        paramOperatorFunc = M.getFunction("_ZNK9AP_ParamTIsL11ap_var_type2EEcvRKsEv");
        paramGetFunc = M.getFunction("_ZNK9AP_ParamTIsL11ap_var_type2EE3getEv");
        paramSetAndSaveFunc = M.getFunction("_ZN9AP_ParamTIsL11ap_var_type2EE12set_and_saveERKs");
    } else if (paramType->isIntegerTy(32)) {
        paramOperatorFunc = M.getFunction("_ZNK9AP_ParamTIiL11ap_var_type3EEcvRKiEv");
        paramGetFunc = M.getFunction("_ZNK9AP_ParamTIiL11ap_var_type3EE3getEv");
        paramSetAndSaveFunc = M.getFunction("_ZN9AP_ParamTIiL11ap_var_type3EE12set_and_saveERKi");
    } else {
        ERROR("Unknown parameter type");
        exit(1);
    }
    assert(paramOperatorFunc != nullptr && "Can't find operator function");
    assert(paramGetFunc != nullptr && "Can't find get function");
    assert(paramSetAndSaveFunc != nullptr && "Can't find set_and_save function");
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

    // we first create a function template
    llvm::FunctionType* funcType = nullptr;
    if (entryArgTypes.size() > 0) {
        funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), entryArgTypes, false);
    } else {
        funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false);
    }
    llvm::Function* func = llvm::Function::Create(funcType, llvm::Function::InternalLinkage, "dump_vars", &M);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", func);
    llvm::IRBuilder<> builder(entry);
    llvm::ArrayType* arrayType = llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 100);
    llvm::AllocaInst* allocaArray = builder.CreateAlloca(arrayType, nullptr, "");
    llvm::AllocaInst* allocaPtr = builder.CreateAlloca(llvm::Type::getInt8PtrTy(ctx), nullptr, "");
    llvm::AllocaInst* alloca1 = builder.CreateAlloca(paramType, nullptr, "");
    // llvm::AllocaInst* alloca2 = builder.CreateAlloca(paramType, nullptr, "");
    // call paramOperatorFunc
    llvm::CallInst* callParamOperator = builder.CreateCall(paramOperatorFunc, {paramNameGV}, "");
    llvm::Value* loadInst = builder.CreateLoad(paramType, callParamOperator, "");
    llvm::Value* sextInst = nullptr;
    if (paramType->isIntegerTy(8) || paramType->isIntegerTy(16)) {
        sextInst = builder.CreateSExt(loadInst, llvm::Type::getInt32Ty(ctx), "");
    }
    llvm::Value* icmpInst = builder.CreateICmpSLE(sextInst ? sextInst : loadInst, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0), "");
    llvm::BasicBlock* falseBB = llvm::BasicBlock::Create(ctx, "false", func);
    llvm::BasicBlock* trueBB = llvm::BasicBlock::Create(ctx, "true", func);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(ctx, "end", func);
    builder.CreateCondBr(icmpInst, trueBB, falseBB);
    // trueBB
    builder.SetInsertPoint(trueBB);
    builder.CreateBr(endBB);
    // falseBB
    builder.SetInsertPoint(falseBB);
    // update curr_timestamp %update_curr_timestamp = call i64 @time(ptr @curr_timestamp) #10
    llvm::GlobalVariable* curr_timestamp = M.getNamedGlobal("curr_timestamp") ? M.getNamedGlobal("curr_timestamp") : createCurrTimestamp(M);
    llvm::Function* timeFunc = M.getFunction("time");
    if (!timeFunc) {
        timeFunc = createTimeFunc(M);
    }
    llvm::CallInst* timeCall = builder.CreateCall(timeFunc, {curr_timestamp}, "update_curr_timestamp");
    timeCall->addFnAttr(llvm::Attribute::NoUnwind);
    // call dump_entry_args(...)
    if (entryArgTypes.size() > 0) {
        std::vector<llvm::Value*> args;
        for (auto &arg : func->args()) {
            args.push_back(&arg);
        }
        llvm::CallInst* callDumpEntryArgs = builder.CreateCall(M.getFunction("dump_entry_args"), args, "");
    }
    // call dump_heap_vars()
    llvm::CallInst* callDumpHeapVars = builder.CreateCall(dumpHeapVarsFunc, {});
    llvm::Value* gep1 = builder.CreateGEP(arrayType, allocaArray, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::Value* gep2 = builder.CreateGEP(arrayType, gep1, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::LoadInst* loadCurrentTime = builder.CreateLoad(llvm::Type::getInt64Ty(ctx), curr_timestamp, "");
    llvm::GlobalVariable* globalLogFileName =  M.getNamedGlobal("globalLogFileName")? M.getNamedGlobal("globalLogFileName") : createStringConstant(M, _curr_property + "_Global_%ld.in", "globalLogFileName");
    llvm::GlobalVariable* wbMode = M.getNamedGlobal("wbMode")? M.getNamedGlobal("wbMode") : createStringConstant(M, "wb", "wbMode");
    // %9 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr noundef gep2, i64 noundef 100, ptr noundef globalLogFileName, i64 noundef loadCurrentTime) #10 attributes #10 = { nounwind }
    llvm::CallInst* callSnprintf = builder.CreateCall(snprintfFunc, {gep2, llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 100), globalLogFileName, loadCurrentTime}, "");
    llvm::Value* gep3 = builder.CreateGEP(arrayType, allocaArray, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::Value* gep4 = builder.CreateGEP(arrayType, gep3, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    // callFopen = call noalias ptr @fopen(ptr noundef gep4, ptr noundef wbMode)
    llvm::CallInst* callFopen = builder.CreateCall(fopenFunc, {gep4, wbMode}, "");
    // store ptr callFopen, ptr allocaPtr, align 8
    llvm::StoreInst* storeFopen = builder.CreateStore(callFopen, allocaPtr);
    // loadFopen = load ptr, ptr allocaPtr, align 8
    llvm::LoadInst* loadFopen = builder.CreateLoad(allocaPtr->getType(), allocaPtr, "");
    // insert fwirite for every global variable
    for (auto &G : M.globals()) {
        // Only collect global variables within 'global_vars.XX' section.
        if (!G.getSection().contains("global_vars.")) {
            continue;
        }
        // for every global variable, we need to create a fwrite call behind the store instruction
        int gsize = M.getDataLayout().getTypeAllocSize(G.getValueType());
        llvm::Type *i64 = llvm::Type::getInt64Ty(M.getContext());
        llvm::Constant* size = llvm::ConstantInt::get(i64, gsize);
        llvm::Constant* one = llvm::ConstantInt::get(i64, 1);
        //  fwriteCall = call i64 @fwrite(ptr noundef &G, i64 noundef size, i64 noundef 1, ptr noundef loadFopen)
        llvm::CallInst* fwriteCall = builder.CreateCall(fwriteFunc, {&G, size, one, loadFopen}, "");
        for (unsigned i = 0; i < fwriteCall->arg_size(); ++i) {
            fwriteCall->addParamAttr(i, llvm::Attribute::NoUndef);
        }
    }
    // callFflush = call i32 @fflush(ptr noundef loadFopen)
    llvm::CallInst* callFflush = builder.CreateCall(fflushFunc, {loadFopen}, "");
    // callFclose = call i32 @fclose(ptr noundef loadFopen)
    llvm::CallInst* callFclose = builder.CreateCall(fcloseFunc, {loadFopen}, "");
    // callParamGet = call fastcc noundef nonnull align 1 dereferenceable(1) ptr paramGetFunc(ptr noundef nonnull align 1 dereferenceable(1) paramNameGV)
    llvm::CallInst* callParamGet = builder.CreateCall(paramGetFunc, {paramNameGV}, "");
    // loadParamGet = load paramType, ptr callParamGet
    llvm::Value* loadParamGet = builder.CreateLoad(paramType, callParamGet, "");
    // sextParamGet = sext paramType loadParamGet to i32
    llvm::Value* sextParamGet = nullptr;
    if (paramType->isIntegerTy(8) || paramType->isIntegerTy(16)) {
        sextParamGet = builder.CreateSExt(loadParamGet, llvm::Type::getInt32Ty(ctx), "");
    }
    // subInst = sub nsw i32 sextParamGet/loadParamGet, 1
    llvm::Value* subInst = builder.CreateSub(sextParamGet ? sextParamGet : loadParamGet, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 1), "");
    llvm::Value* truncInst = nullptr;
    if (paramType->isIntegerTy(8) || paramType->isIntegerTy(16)) {
        truncInst = builder.CreateTrunc(subInst, paramType, "");
    }
    // store paramType subInst, ptr alloca1
    builder.CreateStore(truncInst ? truncInst : subInst, alloca1);
    // call fastcc void @_ZN9AP_ParamTIaL11ap_var_type1EE12set_and_saveERKa(ptr noundef nonnull align 1 dereferenceable(1) paramNameGV, ptr noundef nonnull align 1 dereferenceable(1) alloca1)
    builder.CreateCall(paramSetAndSaveFunc, {paramNameGV, alloca1});
    builder.CreateBr(endBB);
    builder.SetInsertPoint(endBB);
    builder.CreateRetVoid();    // add attributes #1 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
    func->addFnAttr(llvm::Attribute::MustProgress);
    func->addFnAttr(llvm::Attribute::NoInline);
    func->addFnAttr(llvm::Attribute::NoUnwind);
    func->addFnAttr(llvm::Attribute::OptimizeNone);
    // func->addFnAttr(llvm::Attribute::UWTable);
    func->addFnAttr(llvm::Attribute::NoUnwind);
    func->addFnAttr("frame-pointer", "non-leaf");
    func->addFnAttr("no-trapping-math", "true");
    func->addFnAttr("stack-protector-buffer-size", "8");
    func->addFnAttr("target-cpu", "generic");
    func->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    return func;
}

llvm::Function* instrumenter::PX4::createDumpVarsFunc(llvm::Module &M, std::string paramName, std::string paramHandlerName, llvm::Function* entrypoint) {
    llvm::LLVMContext& ctx = M.getContext();

    std::vector<llvm::Type*> entryArgTypes;
    for (auto &arg : entrypoint->args()) {
        entryArgTypes.push_back(arg.getType());
    }
    if (entryArgTypes.size() > 0) {
        // create dump_entry_args(...) function
        llvm::Function* dumpEntryArgsFunc = _inputgetter->createDumpEntryArgsFunc(M, entryArgTypes);
    }
    // create dump_heap_vars() function
    llvm::Function* dumpHeapVarsFunc = _inputgetter->createDumpHeapVarsFunc(M);
    // create dump_vars() function
    llvm::GlobalVariable* paramNameGV = M.getNamedGlobal(paramName);
    if (paramNameGV == nullptr) {
        std::cerr << "Can't find global variable " << paramName << std::endl;
        exit(1);
    }
    llvm::GlobalVariable* paramHandlerNameGV = M.getNamedGlobal(paramHandlerName);
    if (paramHandlerNameGV == nullptr) {
        std::cerr << "Can't find parameter handler " << paramHandlerName << std::endl;
        exit(1);
    }

    llvm::Type* paramType = paramNameGV->getValueType();
    // Unlike ArduCopter, PX4 only has two parameter types (int32_t and float) and only two parameter read/set functions
    llvm::Function* paramGetFunc = M.getFunction("param_get");
    llvm::Function* paramSetFunc = M.getFunction("param_set");
    assert(paramGetFunc != nullptr && "Can't find param_get function");
    assert(paramSetFunc != nullptr && "Can't find param_set function");

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

    // we first create a function template
    llvm::FunctionType* funcType = nullptr;
    if (entryArgTypes.size() > 0) {
        funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), entryArgTypes, false);
    }else {
        funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false);
    }
    llvm::Function* func = llvm::Function::Create(funcType, llvm::Function::InternalLinkage, "dump_vars", &M);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", func);
    llvm::IRBuilder<> builder(entry);
    llvm::ArrayType* arrayType = llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 100);
    llvm::AllocaInst* allocaArray = builder.CreateAlloca(arrayType, nullptr, "");
    llvm::AllocaInst* allocaPtr = builder.CreateAlloca(llvm::Type::getInt8PtrTy(ctx), nullptr, "");
    llvm::AllocaInst* allocaI32 = builder.CreateAlloca(llvm::Type::getInt32Ty(ctx), nullptr, "");
    // %paramHandler = load i16, ptr @paramHandlerName, align 8
    llvm::Value* loadParamHandler = builder.CreateLoad(paramHandlerNameGV->getValueType(), paramHandlerNameGV, "paramHandler");
    // call param_get(param_t param, void *val) to get the parameter value
    llvm::CallInst* callParamGet = builder.CreateCall(paramGetFunc, {loadParamHandler, paramNameGV}, "");
    // load the parameter value
    llvm::Value* loadInst = builder.CreateLoad(paramType, paramNameGV, "");

    llvm::Value* icmpInst = builder.CreateICmpSLE(loadInst, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0), "");
    llvm::BasicBlock* falseBB = llvm::BasicBlock::Create(ctx, "false", func);
    llvm::BasicBlock* trueBB = llvm::BasicBlock::Create(ctx, "true", func);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(ctx, "end", func);
    builder.CreateCondBr(icmpInst, trueBB, falseBB);
    // trueBB
    builder.SetInsertPoint(trueBB);
    builder.CreateBr(endBB);
    // falseBB
    builder.SetInsertPoint(falseBB);
    // update curr_timestamp %update_curr_timestamp = call i64 @time(ptr @curr_timestamp) #10
    llvm::GlobalVariable* curr_timestamp = M.getNamedGlobal("curr_timestamp") ? M.getNamedGlobal("curr_timestamp") : createCurrTimestamp(M);
    llvm::Function* timeFunc = M.getFunction("time");
    if (!timeFunc) {
        timeFunc = createTimeFunc(M);
    }
    llvm::CallInst* timeCall = builder.CreateCall(timeFunc, {curr_timestamp}, "update_curr_timestamp");
    timeCall->addFnAttr(llvm::Attribute::NoUnwind);
    // call dump_entry_args(...) if there are entry arguments
    if (entryArgTypes.size() > 0) {
        std::vector<llvm::Value*> args;
        for (auto &arg : func->args()) {
            args.push_back(&arg);
        }
        llvm::CallInst* callDumpEntryArgs = builder.CreateCall(M.getFunction("dump_entry_args"), args, "");
    }
    // call dump_heap_vars()
    llvm::CallInst* callDumpHeapVars = builder.CreateCall(dumpHeapVarsFunc, {});
    llvm::Value* gep1 = builder.CreateGEP(arrayType, allocaArray, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::Value* gep2 = builder.CreateGEP(arrayType, gep1, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::LoadInst* loadCurrentTime = builder.CreateLoad(llvm::Type::getInt64Ty(ctx), curr_timestamp, "");
    llvm::GlobalVariable* globalLogFileName =  M.getNamedGlobal("globalLogFileName")? M.getNamedGlobal("globalLogFileName") : createStringConstant(M, _curr_property + "_Global_%ld.in", "globalLogFileName");
    llvm::GlobalVariable* wbMode = M.getNamedGlobal("wbMode")? M.getNamedGlobal("wbMode") : createStringConstant(M, "wb", "wbMode");
    // %9 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr noundef gep2, i64 noundef 100, ptr noundef globalLogFileName, i64 noundef loadCurrentTime) #10 attributes #10 = { nounwind }
    llvm::CallInst* callSnprintf = builder.CreateCall(snprintfFunc, {gep2, llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 100), globalLogFileName, loadCurrentTime}, "");
    llvm::Value* gep3 = builder.CreateGEP(arrayType, allocaArray, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    llvm::Value* gep4 = builder.CreateGEP(arrayType, gep3, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0)});
    // callFopen = call noalias ptr @fopen(ptr noundef gep4, ptr noundef wbMode)
    llvm::CallInst* callFopen = builder.CreateCall(fopenFunc, {gep4, wbMode}, "");
    // store ptr callFopen, ptr allocaPtr, align 8
    llvm::StoreInst* storeFopen = builder.CreateStore(callFopen, allocaPtr);
    // loadFopen = load ptr, ptr allocaPtr, align 8
    llvm::LoadInst* loadFopen = builder.CreateLoad(allocaPtr->getType(), allocaPtr, "");
    // insert fwirite for every global variable
    for (auto &G : M.globals()) {
        // Only collect global variables within 'global_vars.XX' section.
        if (!G.getSection().contains("global_vars.")) {
            continue;
        }
        // for every global variable, we need to create a fwrite call behind the store instruction
        int gsize = M.getDataLayout().getTypeAllocSize(G.getValueType());
        llvm::Type *i64 = llvm::Type::getInt64Ty(M.getContext());
        llvm::Constant* size = llvm::ConstantInt::get(i64, gsize);
        llvm::Constant* one = llvm::ConstantInt::get(i64, 1);
        //  fwriteCall = call i64 @fwrite(ptr noundef &G, i64 noundef size, i64 noundef 1, ptr noundef loadFopen)
        llvm::CallInst* fwriteCall = builder.CreateCall(fwriteFunc, {&G, size, one, loadFopen}, "");
        for (unsigned i = 0; i < fwriteCall->arg_size(); ++i) {
            fwriteCall->addParamAttr(i, llvm::Attribute::NoUndef);
        }
    }
    // callFflush = call i32 @fflush(ptr noundef loadFopen)
    llvm::CallInst* callFflush = builder.CreateCall(fflushFunc, {loadFopen}, "");
    // callFclose = call i32 @fclose(ptr noundef loadFopen)
    llvm::CallInst* callFclose = builder.CreateCall(fcloseFunc, {loadFopen}, "");

    llvm::Value* paramValue = loadInst;
    // subInst = sub nsw i32 paramValue, 1
    llvm::Value* subInst = builder.CreateSub(loadInst, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 1), "");
    // store i32 subInst, ptr @paramValue
    builder.CreateStore(subInst, paramNameGV);
    // load paramHandler again
    llvm::Value* loadParamHandler2 = builder.CreateLoad(paramHandlerNameGV->getValueType(), paramHandlerNameGV, "paramHandler2");
    // call param_set(param_t param, const void *val) to set the parameter value
    builder.CreateCall(paramSetFunc, {loadParamHandler2, paramNameGV});
    builder.CreateBr(endBB);
    builder.SetInsertPoint(endBB);
    builder.CreateRetVoid();    // add attributes #1 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
    func->addFnAttr(llvm::Attribute::MustProgress);
    func->addFnAttr(llvm::Attribute::NoInline);
    func->addFnAttr(llvm::Attribute::NoUnwind);
    func->addFnAttr(llvm::Attribute::OptimizeNone);
    // func->addFnAttr(llvm::Attribute::UWTable);
    func->addFnAttr(llvm::Attribute::NoUnwind);
    func->addFnAttr("frame-pointer", "non-leaf");
    func->addFnAttr("no-trapping-math", "true");
    func->addFnAttr("stack-protector-buffer-size", "8");
    func->addFnAttr("target-cpu", "generic");
    func->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    return func;
}

// The parameter of ArduCopter is a struct variable of type AP_ParamI32
void instrumenter::ArduCopter::insertCounter(llvm::Function* entrypoint, std::string cycle) {

    llvm::LLVMContext &context = entrypoint->getContext();
    llvm::BasicBlock &originalEntryBlock = entrypoint->getEntryBlock();
    
    llvm::BasicBlock* newEntryBlock = llvm::BasicBlock::Create(context, "counter_entry", entrypoint, &originalEntryBlock);
    llvm::IRBuilder<> builder(newEntryBlock);

    // Load the counter variable
    llvm::GlobalVariable* counter = entrypoint->getParent()->getNamedGlobal("counter");
    if (!counter) {
        counter = createCounter(*entrypoint->getParent());
    }
    llvm::LoadInst* loadCounter = builder.CreateLoad(counter->getValueType(), counter, "loadCounter");

    // Only the param rate is larger than 0, we increment the counter
    llvm::GlobalVariable* rateVar = entrypoint->getParent()->getNamedGlobal(cycle);
    assert(rateVar && "Can't find global variable for rate");
    
    llvm::Value* indices[] = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), 
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0) 
    };
    llvm::Value* gepPtr = builder.CreateInBoundsGEP(rateVar->getValueType(), rateVar, indices, "cycleValuePtr");
    llvm::LoadInst* cycleValue = builder.CreateLoad(llvm::Type::getInt32Ty(context), gepPtr, "cycleValue");
    
    llvm::Value* isRatePositive = builder.CreateICmpSGT(cycleValue, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "isRatePositive");
    
    llvm::BasicBlock* incrementBlock = llvm::BasicBlock::Create(context, "increment", entrypoint, &originalEntryBlock);
    builder.CreateCondBr(isRatePositive, incrementBlock, &originalEntryBlock);
    
    // Increment block
    builder.SetInsertPoint(incrementBlock);
    // Increment the counter
    llvm::Value* incrementedCounter = builder.CreateAdd(loadCounter, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1), "incrementedCounter");
    // Store the incremented value back to the counter variable
    builder.CreateStore(incrementedCounter, counter);

    // Check if counter == rate, if so reset to 1
    llvm::Value* shouldReset = builder.CreateICmpEQ(incrementedCounter, cycleValue, "shouldReset");
    
    llvm::BasicBlock* resetBlock = llvm::BasicBlock::Create(context, "resetCounter", entrypoint, &originalEntryBlock);
    
    builder.CreateCondBr(shouldReset, resetBlock, &originalEntryBlock);
    
    // Reset block: set counter to 0
    builder.SetInsertPoint(resetBlock);
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), counter);
    builder.CreateBr(&originalEntryBlock);
}

// The parameter of PX4 is a integer variable
void instrumenter::PX4::insertCounter(llvm::Function* entrypoint, std::string cycle) {
    llvm::LLVMContext &context = entrypoint->getContext();
    llvm::BasicBlock &originalEntryBlock = entrypoint->getEntryBlock();
    
    llvm::BasicBlock* newEntryBlock = llvm::BasicBlock::Create(context, "counter_entry", entrypoint, &originalEntryBlock);
    llvm::IRBuilder<> builder(newEntryBlock);

    // Load the counter variable
    llvm::GlobalVariable* counter = entrypoint->getParent()->getNamedGlobal("counter");
    if (!counter) {
        counter = createCounter(*entrypoint->getParent());
    }
    llvm::LoadInst* loadCounter = builder.CreateLoad(counter->getValueType(), counter, "loadCounter");

    // Only the param rate is larger than 0, we increment the counter
    llvm::GlobalVariable* rateVar = entrypoint->getParent()->getNamedGlobal(cycle);
    assert(rateVar && "Can't find global variable for rate");

    llvm::LoadInst* cycleValue = builder.CreateLoad(llvm::Type::getInt32Ty(context), rateVar, "cycleValue");

    llvm::Value* isRatePositive = builder.CreateICmpSGT(cycleValue, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "isRatePositive");
    
    llvm::BasicBlock* incrementBlock = llvm::BasicBlock::Create(context, "increment", entrypoint, &originalEntryBlock);
    builder.CreateCondBr(isRatePositive, incrementBlock, &originalEntryBlock);
    
    // Increment block
    builder.SetInsertPoint(incrementBlock);
    // Increment the counter
    llvm::Value* incrementedCounter = builder.CreateAdd(loadCounter, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1), "incrementedCounter");
    // Store the incremented value back to the counter variable
    builder.CreateStore(incrementedCounter, counter);

    // Check if counter == rate, if so reset to 1
    llvm::Value* shouldReset = builder.CreateICmpEQ(incrementedCounter, cycleValue, "shouldReset");
    
    llvm::BasicBlock* resetBlock = llvm::BasicBlock::Create(context, "resetCounter", entrypoint, &originalEntryBlock);
    
    builder.CreateCondBr(shouldReset, resetBlock, &originalEntryBlock);
    
    // Reset block: set counter to 0
    builder.SetInsertPoint(resetBlock);
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), counter);
    builder.CreateBr(&originalEntryBlock);
}

// declare i32 @snprintf(ptr noundef, i64 noundef, ptr noundef, ...) local_unnamed_addr #0 attributes #0 = { nounwind "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
llvm::Function* instrumenter::createSnprintf(llvm::Module &M){
    llvm::LLVMContext &context = M.getContext();
    llvm::Type* i32 = llvm::Type::getInt32Ty(context);
    llvm::Type* i64 = llvm::Type::getInt64Ty(context);
    llvm::Type* i8Ptr = llvm::Type::getInt8PtrTy(context);
    llvm::FunctionType* snprintfType = llvm::FunctionType::get(i32, {i8Ptr, i64, i8Ptr}, true);
    llvm::Function* snprintfFunc = llvm::Function::Create(snprintfType, llvm::Function::ExternalLinkage, "snprintf", &M);
    // add attributes for args
    for (unsigned i = 0; i < snprintfFunc->arg_size(); ++i) {
        snprintfFunc->addParamAttr(i, llvm::Attribute::AttrKind::NoUndef);
    }
    // add attributes
    snprintfFunc->addFnAttr("frame-pointer", "non-leaf");
    snprintfFunc->addFnAttr("no-trapping-math", "true");
    snprintfFunc->addFnAttr("stack-protector-buffer-size", "8");
    snprintfFunc->addFnAttr("target-cpu", "generic");
    snprintfFunc->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    snprintfFunc->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Local);
    return snprintfFunc;
}

// declare noalias ptr @fopen(ptr noundef, ptr noundef) local_unnamed_addr #11 attributes #11 = { "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
llvm::Function* instrumenter::createFopenFunc(llvm::Module &M){
    llvm::LLVMContext &context = M.getContext();
    llvm::Type* i8Ptr = llvm::Type::getInt8PtrTy(context);
    llvm::FunctionType* fopenType = llvm::FunctionType::get(i8Ptr, {i8Ptr, i8Ptr}, false);
    llvm::Function* fopenFunc = llvm::Function::Create(fopenType, llvm::Function::ExternalLinkage, "fopen", &M);
    // add attributes for args
    for (unsigned i = 0; i < fopenFunc->arg_size(); ++i) {
        fopenFunc->addParamAttr(i, llvm::Attribute::AttrKind::NoUndef);
    }
    // add attributes
    fopenFunc->addFnAttr("frame-pointer", "non-leaf");
    fopenFunc->addFnAttr("no-trapping-math", "true");
    fopenFunc->addFnAttr("stack-protector-buffer-size", "8");
    fopenFunc->addFnAttr("target-cpu", "generic");
    fopenFunc->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    fopenFunc->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Local);
    return fopenFunc;
}

// declare i32 @setvbuf(ptr noundef, ptr noundef, i32 noundef, i64 noundef) #3 attributes #3 = { nounwind "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
llvm::Function* instrumenter::createSetvbufFunc(llvm::Module &M) {
    llvm::LLVMContext &context = M.getContext();
    llvm::Type* i32 = llvm::Type::getInt32Ty(context);
    llvm::Type* i64 = llvm::Type::getInt64Ty(context);
    llvm::Type* i8Ptr = llvm::Type::getInt8PtrTy(context);
    llvm::FunctionType* setvbuffType = llvm::FunctionType::get(i32, {i8Ptr, i8Ptr, i32, i64}, false);
    llvm::Function* setvbuffFunc = llvm::Function::Create(setvbuffType, llvm::Function::ExternalLinkage, "setvbuf", &M);
    // add attributes for args
    for (unsigned i = 0; i < setvbuffFunc->arg_size(); ++i) {
        setvbuffFunc->addParamAttr(i, llvm::Attribute::AttrKind::NoUndef);
    }
    // add attributes
    setvbuffFunc->addFnAttr("frame-pointer", "non-leaf");
    setvbuffFunc->addFnAttr("no-trapping-math", "true");
    setvbuffFunc->addFnAttr("stack-protector-buffer-size", "8");
    setvbuffFunc->addFnAttr("target-cpu", "generic");
    setvbuffFunc->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    return setvbuffFunc;
}

// create a fwrite function declaration
llvm::Function* instrumenter::createFwriteFunc(llvm::Module &M){
    llvm::LLVMContext &context = M.getContext();
    llvm::Type* i64 = llvm::Type::getInt64Ty(context);
    llvm::Type* i8Ptr = llvm::Type::getInt8PtrTy(context);
    llvm::FunctionType* fwriteType = llvm::FunctionType::get(i64, {i8Ptr, i64, i64, i8Ptr}, false);
    llvm::Function* fwriteFunc = llvm::Function::Create(fwriteType, llvm::Function::ExternalLinkage, "fwrite", &M);
    // add attributes for args
    for (unsigned i = 0; i < fwriteFunc->arg_size(); ++i) {
        fwriteFunc->addParamAttr(i, llvm::Attribute::AttrKind::NoUndef);
    }
    // add attributes
    fwriteFunc->addFnAttr("frame-pointer", "non-leaf");
    fwriteFunc->addFnAttr("no-trapping-math", "true");
    fwriteFunc->addFnAttr("stack-protector-buffer-size", "8");
    fwriteFunc->addFnAttr("target-cpu", "generic");
    fwriteFunc->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    return fwriteFunc;
}

// create a fflush function declaration
// declare i32 @fflush(ptr noundef) local_unnamed_addr 
llvm::Function* instrumenter::createFflushFunc(llvm::Module &M){
    llvm::LLVMContext &context = M.getContext();
    llvm::Type* i32 = llvm::Type::getInt32Ty(context);
    llvm::Type* i8Ptr = llvm::Type::getInt8PtrTy(context);
    llvm::FunctionType* fflushType = llvm::FunctionType::get(i32, {i8Ptr}, false);
    llvm::Function* fflushFunc = llvm::Function::Create(fflushType, llvm::Function::ExternalLinkage, "fflush", &M);
    // add attributes for args
    for (unsigned i = 0; i < fflushFunc->arg_size(); ++i) {
        fflushFunc->addParamAttr(i, llvm::Attribute::AttrKind::NoUndef);
    }
    fflushFunc->addFnAttr("frame-pointer", "non-leaf");
    fflushFunc->addFnAttr("no-trapping-math", "true");
    fflushFunc->addFnAttr("stack-protector-buffer-size", "8");
    fflushFunc->addFnAttr("target-cpu", "generic");
    fflushFunc->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    fflushFunc->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Local);
    return fflushFunc;
}

// create a fclose function declaration
// declare i32 @fclose(ptr noundef) local_unnamed_addr
llvm::Function* instrumenter::createFcloseFunc(llvm::Module &M){
    llvm::LLVMContext &context = M.getContext();
    llvm::Type* i32 = llvm::Type::getInt32Ty(context);
    llvm::Type* i8Ptr = llvm::Type::getInt8PtrTy(context);
    llvm::FunctionType* fcloseType = llvm::FunctionType::get(i32, {i8Ptr}, false);
    llvm::Function* fcloseFunc = llvm::Function::Create(fcloseType, llvm::Function::ExternalLinkage, "fclose", &M);
    // add attributes for args
    for (unsigned i = 0; i < fcloseFunc->arg_size(); ++i) {
        fcloseFunc->addParamAttr(i, llvm::Attribute::AttrKind::NoUndef);
    }
    fcloseFunc->addFnAttr("frame-pointer", "non-leaf");
    fcloseFunc->addFnAttr("no-trapping-math", "true");
    fcloseFunc->addFnAttr("stack-protector-buffer-size", "8");
    fcloseFunc->addFnAttr("target-cpu", "generic");
    fcloseFunc->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    fcloseFunc->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Local);
    return fcloseFunc;
}

// declare i64 @time(ptr noundef) local_unnamed_addr #0
// attributes #0 = { nounwind "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
llvm::Function* instrumenter::createTimeFunc(llvm::Module &M){
    llvm::LLVMContext &context = M.getContext();
    llvm::Type* i64 = llvm::Type::getInt64Ty(context);
    llvm::FunctionType* timeType = llvm::FunctionType::get(i64, {i64}, false);
    llvm::Function* timeFunc = llvm::Function::Create(timeType, llvm::Function::ExternalLinkage, "time", &M);
    // add attributes for args
    for (unsigned i = 0; i < timeFunc->arg_size(); ++i) {
        timeFunc->addParamAttr(i, llvm::Attribute::AttrKind::NoUndef);
    }
    // add attributes
    timeFunc->addFnAttr("frame-pointer", "non-leaf");
    timeFunc->addFnAttr("no-trapping-math", "true");
    timeFunc->addFnAttr("stack-protector-buffer-size", "8");
    timeFunc->addFnAttr("target-cpu", "generic");
    timeFunc->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    return timeFunc;
}

// declare i32 @getpid() local_unnamed_addr #0
// attributes #0 = { nounwind "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+outline-atomics,+v8a,-fmv" }
llvm::Function* instrumenter::createGetpidFunc(llvm::Module &M){
    llvm::LLVMContext &context = M.getContext();
    llvm::Type* i32 = llvm::Type::getInt32Ty(context);
    llvm::FunctionType* getpidType = llvm::FunctionType::get(i32, false);
    llvm::Function* getpidFunc = llvm::Function::Create(getpidType, llvm::Function::ExternalLinkage, "getpid", &M);
    // add attributes
    getpidFunc->addFnAttr("frame-pointer", "non-leaf");
    getpidFunc->addFnAttr("no-trapping-math", "true");
    getpidFunc->addFnAttr("stack-protector-buffer-size", "8");
    getpidFunc->addFnAttr("target-cpu", "generic");
    getpidFunc->addFnAttr("target-features", "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv");
    return getpidFunc;
}

uint32_t instrumenter::getBBNum(std::string labelstr){
    // Extract the number after "bbNum"
    size_t subPos = labelstr.find("_sub");
    std::string numstr;
    if (subPos != std::string::npos) {
        numstr = labelstr.substr(5, subPos - 5);
    } else {
        numstr = labelstr.substr(5); // Extract from position 5 to end if no "_sub"
    }
    return std::stoi(numstr);
}

// uint32_t instrumenter::getSubBBNum(std::string labelstr){
//     // Extract the number after "_sub"
//     size_t pos = labelstr.find("_sub");
//     std::string numstr = labelstr.substr(pos+4);
//     return std::stoi(numstr);
// }

/**
 * get the UID of the value.
 * if the value is a call instruction, we get the inCallID metadata.
 * if the value is a function, we get the funcID metadata.
 * otherwise, we return -1.
 */
int instrumenter::getUID(llvm::Value* val){
    int ret = -1;
    if (auto *callInst = llvm::dyn_cast<llvm::CallBase>(val)){
        // we get inCallID metadata
        llvm::MDNode* inCallID = callInst->getMetadata("inCallID");
        if (inCallID){
            llvm::Metadata* md = inCallID->getOperand(0).get();
            llvm::ConstantAsMetadata* cam = llvm::dyn_cast<llvm::ConstantAsMetadata>(md);
            llvm::ConstantInt* ci = llvm::dyn_cast<llvm::ConstantInt>(cam->getValue());
            ret = ci->getSExtValue();
        }
    }else if (auto *func = llvm::dyn_cast<llvm::Function>(val)){
        // we get funcID metadata
        llvm::MDNode* funcID = func->getMetadata("funcID");
        if (funcID){
            llvm::Metadata* md = funcID->getOperand(0).get();
            llvm::ConstantAsMetadata* cam = llvm::dyn_cast<llvm::ConstantAsMetadata>(md);
            llvm::ConstantInt* ci = llvm::dyn_cast<llvm::ConstantInt>(cam->getValue());
            ret = ci->getSExtValue();
        }
    }
    return ret;
}