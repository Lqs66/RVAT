#include "instrumenter.hh"
#include <iostream>
#include "llvm/Support/GraphWriter.h"

using namespace llvm;

// for test
namespace llvm {
    template <>
    struct DOTGraphTraits<llvm::Function*> : public DefaultDOTGraphTraits {
        DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

        std::string getNodeLabel(const BasicBlock *Node, const Function *Graph) {
            std::string Label;
            raw_string_ostream OS(Label);

            OS << Node->getName() << "\\l";

            for (const auto &Inst : *Node) {
                Inst.print(OS);
                OS << "\\l";
            }

            return OS.str();
        }

        std::string getNodeAttributes(const BasicBlock *Node, const Function *Graph){
            if (Node->getName().startswith("bbNum")) {
                return "color=pink, style=filled";
            }else{
                return "";
            }
        }
    };
} // namespace llvm

void dumpCFG(llvm::Function &F, const std::string &fileName) {
    std::error_code EC;
    llvm::raw_fd_ostream file(fileName, EC, llvm::sys::fs::OF_Text);

    if (EC) {
        llvm::errs() << "Error opening file: " << EC.message() << "\n";
        return;
    }

    llvm::WriteGraph(file, &F);
    llvm::errs() << "CFG written to " << fileName << "\n";
}

bool isSplitedCallBB(llvm::BasicBlock &BB) {
    if (BB.size() == 2){
        llvm::Instruction* firstInst = &*BB.begin();
        if (auto callInst = llvm::dyn_cast<llvm::CallBase>(firstInst)) {
            if (callInst->isIndirectCall()) {
                return true;
            }else{
                if (auto callFunc = callInst->getCalledFunction()) {
                    if (!callFunc->isDeclaration()) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void instrumenter::BasicBlockTimer::createThreadLocalGlobalVariables(llvm::Module &M) {
    // @fileNamePattern = dso_local global [12 x i8] c"_curr_property + _Time_%d.in\00", align 1
    llvm::GlobalVariable* fileNamePattern = M.getNamedGlobal("fileNamePattern");
    if (!fileNamePattern) {
        fileNamePattern = createStringConstant(M, _curr_property + "_Time_%d.in", "fileNamePattern");
        fileNamePattern->setLinkage(llvm::GlobalValue::ExternalLinkage);
    }
    // @timeLogFilePtr = internal thread_local global ptr zeroinitializer, align 8
    llvm::GlobalVariable* timeLogFilePtr = M.getNamedGlobal("timeLogFilePtr");
    if (!timeLogFilePtr) {
        timeLogFilePtr = createGlobalPointer(M, "timeLogFilePtr");
        timeLogFilePtr->setThreadLocalMode(llvm::GlobalValue::ThreadLocalMode::GeneralDynamicTLSModel);
        timeLogFilePtr->setLinkage(llvm::GlobalValue::ExternalLinkage);
    }
    // @timeCounter = internal thread_local global i32 0, align 8
    llvm::GlobalVariable* timeCounter = M.getNamedGlobal("timeCounter");
    if (!timeCounter) {
        timeCounter = new llvm::GlobalVariable(M, llvm::Type::getInt32Ty(M.getContext()), false, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(llvm::Type::getInt32Ty(M.getContext()), 0), "timeCounter");
        timeCounter->setThreadLocalMode(llvm::GlobalValue::ThreadLocalMode::GeneralDynamicTLSModel);
        timeCounter->setLinkage(llvm::GlobalValue::ExternalLinkage);
    }
}

llvm::Function* instrumenter::ArduCopter::createTimeLoggerCaller(llvm::Module &M, std::string cycleParam) {
    llvm::Function* time_logger = M.getFunction("time_logger");
    if (!time_logger) {
        llvm::LLVMContext &ctx = M.getContext();
        llvm::FunctionType *voidTy = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {llvm::Type::getInt64Ty(ctx), llvm::Type::getInt64Ty(ctx)}, false);
        time_logger = llvm::Function::Create(voidTy, llvm::Function::ExternalLinkage, "time_logger", &M);
    }

    llvm::LLVMContext &ctx = M.getContext();
    llvm::FunctionType *callerTy = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {llvm::Type::getInt64Ty(ctx), llvm::Type::getInt64Ty(ctx)}, false);
    llvm::Function *time_logger_caller = llvm::Function::Create(callerTy, llvm::Function::InternalLinkage, "time_logger_caller", &M);
    llvm::Argument* bbNum_subNum = &*time_logger_caller->arg_begin();
    llvm::Argument* exeTime = &*(time_logger_caller->arg_begin() + 1);
    bbNum_subNum->setName("bbNum_subNum");
    exeTime->setName("exeTime");
    
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ctx, "entry", time_logger_caller);
    llvm::IRBuilder<> builder(entry);

    // get @time_c_cycle value
    llvm::GlobalVariable* time_c_cycle = M.getNamedGlobal(cycleParam);
    assert(time_c_cycle && "Global variable time_c_cycle does not exist.");
    llvm::Value* time_c_cycle_ptr = builder.CreateGEP(time_c_cycle->getValueType(), time_c_cycle, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0)}, "time_c_cycle_ptr");
    llvm::LoadInst* time_c_cycle_load = builder.CreateLoad(llvm::Type::getInt32Ty(ctx), time_c_cycle_ptr, "time_c_cycle_load");

    // get @timeCounter value
    llvm::GlobalVariable* timeCounter = M.getNamedGlobal("timeCounter");
    assert(timeCounter && "Global variable timeCounter does not exist.");
    llvm::CallInst* threadLocalTimeCounter = builder.CreateThreadLocalAddress(timeCounter);
    llvm::LoadInst* timeCounterLoad = builder.CreateLoad(llvm::Type::getInt32Ty(ctx), threadLocalTimeCounter, "timeCounterLoad");

    llvm::Value* isEqual = builder.CreateICmpEQ(timeCounterLoad, time_c_cycle_load, "isEqual");
    llvm::BasicBlock* callLogger = llvm::BasicBlock::Create(ctx, "callLogger", time_logger_caller);
    llvm::BasicBlock* incrementOnly = llvm::BasicBlock::Create(ctx, "incrementOnly", time_logger_caller);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(ctx, "end", time_logger_caller);
    
    builder.CreateCondBr(isEqual, callLogger, incrementOnly);

    // callLogger: call time_logger and reset the counter
    builder.SetInsertPoint(callLogger);
    builder.CreateCall(time_logger, {bbNum_subNum, exeTime});
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 1), threadLocalTimeCounter);
    builder.CreateBr(end);

    // increment only
    builder.SetInsertPoint(incrementOnly);
    llvm::Value* incrementedCounter = builder.CreateAdd(timeCounterLoad, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 1), "incrementedCounter");
    builder.CreateStore(incrementedCounter, threadLocalTimeCounter);
    builder.CreateBr(end);

    builder.SetInsertPoint(end);
    builder.CreateRetVoid();
    
    return time_logger_caller;
}

llvm::Function* instrumenter::PX4::createTimeLoggerCaller(llvm::Module &M, std::string cycleParam) {
    llvm::Function* time_logger = M.getFunction("time_logger");
    if (!time_logger) {
        llvm::LLVMContext &ctx = M.getContext();
        llvm::FunctionType *voidTy = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {llvm::Type::getInt64Ty(ctx), llvm::Type::getInt64Ty(ctx)}, false);
        time_logger = llvm::Function::Create(voidTy, llvm::Function::ExternalLinkage, "time_logger", &M);
    }

    llvm::LLVMContext &ctx = M.getContext();
    llvm::FunctionType *callerTy = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {llvm::Type::getInt64Ty(ctx), llvm::Type::getInt64Ty(ctx)}, false);
    llvm::Function *time_logger_caller = llvm::Function::Create(callerTy, llvm::Function::InternalLinkage, "time_logger_caller", &M);
    llvm::Argument* bbNum_subNum = &*time_logger_caller->arg_begin();
    llvm::Argument* exeTime = &*(time_logger_caller->arg_begin() + 1);
    bbNum_subNum->setName("bbNum_subNum");
    exeTime->setName("exeTime");
    
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ctx, "entry", time_logger_caller);
    llvm::IRBuilder<> builder(entry);

    // get @time_c_cycle value
    llvm::GlobalVariable* time_c_cycle = M.getNamedGlobal(cycleParam);
    assert(time_c_cycle && "Global variable _time_c_cycle does not exist.");
    llvm::LoadInst* time_c_cycle_load = builder.CreateLoad(llvm::Type::getInt32Ty(ctx), time_c_cycle, "time_c_cycle_load");

    // get @timeCounter value
    llvm::GlobalVariable* timeCounter = M.getNamedGlobal("timeCounter");
    assert(timeCounter && "Global variable timeCounter does not exist.");
    llvm::CallInst* threadLocalTimeCounter = builder.CreateThreadLocalAddress(timeCounter);
    llvm::LoadInst* timeCounterLoad = builder.CreateLoad(llvm::Type::getInt32Ty(ctx), threadLocalTimeCounter, "timeCounterLoad");

    llvm::Value* isEqual = builder.CreateICmpEQ(timeCounterLoad, time_c_cycle_load, "isEqual");
    llvm::BasicBlock* callLogger = llvm::BasicBlock::Create(ctx, "callLogger", time_logger_caller);
    llvm::BasicBlock* incrementOnly = llvm::BasicBlock::Create(ctx, "incrementOnly", time_logger_caller);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(ctx, "end", time_logger_caller);
    
    builder.CreateCondBr(isEqual, callLogger, incrementOnly);

    // callLogger: call time_logger and reset the counter
    builder.SetInsertPoint(callLogger);
    builder.CreateCall(time_logger, {bbNum_subNum, exeTime});
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 1), threadLocalTimeCounter);
    builder.CreateBr(end);

    // increment only
    builder.SetInsertPoint(incrementOnly);
    llvm::Value* incrementedCounter = builder.CreateAdd(timeCounterLoad, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 1), "incrementedCounter");
    builder.CreateStore(incrementedCounter, threadLocalTimeCounter);
    builder.CreateBr(end);

    builder.SetInsertPoint(end);
    builder.CreateRetVoid();
    
    return time_logger_caller;
}

// void instrumenter::BasicBlockTimer::splitCallSitesInBlock(llvm::BasicBlock& BB) {
//     std::string bbName = BB.getName().str();
//     BB.setName(bbName + "_sub0");
    
//     // Add the original block
//     _splitBBNeedToTimer.insert(&BB);
    
//     // Prepare split points array, split points should be the position of call instructions and the position of the next instruction.
//     std::vector<int> splitPoints;
//     int i = 0;
//     std::vector<int> callSites; // target call sites
    
//     for(auto& instr : BB){
//         // we only split non-intrinsic call instructions
//         if (auto callInst = llvm::dyn_cast<llvm::CallBase>(&instr)) {
//             if (callInst->isIndirectCall()) {
//                 callSites.push_back(i);
//             } else {
//                 if (auto calledFunc = callInst->getCalledFunction()) {
//                     if (!calledFunc->isIntrinsic()) {
//                         callSites.push_back(i);
//                     }
//                 }
//             }
//         }
//         i++;
//     }

//     if(callSites.size() == 0){
//         return;
//     }

//     // Add the position of each call instruction and the position of the next instruction as split points
//     for(int i = 0; i < callSites.size(); i++){
//         splitPoints.push_back(callSites[i]);
//         splitPoints.push_back(callSites[i]+1);
//     }

//     // Split the basic block according to the split points array
//     auto it = BB.begin();
//     llvm::BasicBlock* toSplit = &BB;
//     int currSize = BB.size();
//     int last = 0; // last split point
//     uint32_t subBlockID = 0;
    
//     for(int i = 0; i < splitPoints.size() ; i++){
//         for(int j = 0; j < splitPoints[i] - last ; j++){
//             it++;
//         }
//         if(splitPoints[i] == 0 || splitPoints[i] == currSize - 1){
//             continue;
//         }
//         last = splitPoints[i];
//         if(toSplit->size() <= 2)
//             continue;
//         toSplit = toSplit->splitBasicBlock(it);
//         toSplit->setName(bbName + "_sub" + std::to_string(++subBlockID));
        
//         // Add the newly created split block
//         _splitBBNeedToTimer.insert(toSplit);
//     }
// }

/** 
 * Use the builtin intrinsic '@llvm.readcyclecounter() #1 attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }' to obtain the current timestamp
 */
void instrumenter::BasicBlockTimer::insertTimeLoggerCallerIntoBasicBlock(llvm::BasicBlock& BB) {
    
    // we need to insert %start_time = call i64 @llvm.readcyclecounter() to record the start time of the basic block into the beginning of the basic block
    // if curr BB only has terminator, we skip it
    // we also need to skip the BB which only has call instruction
    if (BB.getFirstNonPHI() == BB.getTerminator() || isSplitedCallBB(BB)) {
        return;
    }
    llvm::IRBuilder<> builder(BB.getFirstNonPHI());
    llvm::Function *readCycleCounter = llvm::Intrinsic::getDeclaration(BB.getParent()->getParent(), llvm::Intrinsic::readcyclecounter);
    llvm::CallInst* start_time = builder.CreateCall(readCycleCounter);
    start_time->setName("start_time");

    uint32_t bbNum = getBBNum(BB.getName().str());
    // uint32_t subBBNum = getSubBBNum(BB.getName().str());
    // // bbNum as high 32 bits, subBBNo as low 32 bits, little-endian
    // uint64_t id = ((uint64_t)bbNum << 32) | subBBNum;
    uint32_t id = bbNum;

    // we insert insert %end_time = call i64 @llvm.readcyclecounter() to record the end time of the basic block
    builder.SetInsertPoint(BB.getTerminator());
    llvm::CallInst* end_time = builder.CreateCall(readCycleCounter);
    end_time->setName("end_time");
    // insert %diff_time = sub i64 %end_time, %start_time
    llvm::Value* diff_time = builder.CreateSub(end_time, start_time, "diff_time");

    // call time_logger_caller(i64 %bbNum_subNum, i64 %exeTime)
    llvm::Function* time_logger_caller = BB.getParent()->getParent()->getFunction("time_logger_caller");
    if (!time_logger_caller) {
        // time_logger_caller = createTimeLoggerCaller(*BB.getParent()->getParent());
        std::cerr << "Error: time_logger_caller function does not exist." << std::endl;
        exit(1);
    }
    // create the call instruction
    llvm::CallInst* callTimeLogger = builder.CreateCall(time_logger_caller, {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(BB.getContext()), id),
        diff_time});
    callTimeLogger->setDoesNotThrow();
}

// void instrumenter::BasicBlockTimer::insertTimeLoggerIntoFunction(llvm::Function& F){
//     // we first insert following code into the beginning of the function
//     // %buffer = alloca [2 x i64], align 8
//     // %buffer_idx = alloca i32, align 4
//     // store i32 2, ptr %buffer_idx
//     // %buffer_addr = getelementptr inbounds [2 x i64], ptr %buffer, i64 0, i64 0
//     // we store id into buffer[0], store i64 %id, ptr %buffer_addr
//     // start_time = call i64 @llvm.readcyclecounter()
//     llvm::IRBuilder<> builder(F.getEntryBlock().getFirstNonPHI());
//     llvm::ArrayType* bufferType = llvm::ArrayType::get(llvm::Type::getInt64Ty(F.getContext()), 2);
//     llvm::AllocaInst* buffer = builder.CreateAlloca(bufferType, nullptr, "buffer");
//     llvm::AllocaInst* buffer_idx = builder.CreateAlloca(llvm::Type::getInt32Ty(F.getContext()), nullptr, "buffer_idx");
//     builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(F.getContext()), 2), buffer_idx);
//     uint64_t fid = getUID(&F);
//     llvm::Value* buffer_addr = builder.CreateGEP(bufferType, buffer, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(F.getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(F.getContext()), 0)}, "buffer_addr");
//     builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(F.getContext()), fid), buffer_addr);
//     llvm::Function *readCycleCounter = llvm::Intrinsic::getDeclaration(F.getParent(), llvm::Intrinsic::readcyclecounter);
//     llvm::CallInst* start_time = builder.CreateCall(readCycleCounter);
//     start_time->setName("start_time");

//     for (auto &BB : F){
//         // if BB's terminator is a return instruction, we insert time_logger before the return instruction
//         if (llvm::isa<llvm::ReturnInst>(BB.getTerminator())){
//             builder.SetInsertPoint(BB.getTerminator());
//             // insert %end_time = call i64 @llvm.readcyclecounter()
//             // %diff_time = sub i64 %end_time, %start_time
//             llvm::CallInst* end_time = builder.CreateCall(readCycleCounter);
//             end_time->setName("end_time");
//             llvm::Value* diff_time = builder.CreateSub(end_time, start_time, "diff_time");
//             // store diff_time into buffer[1]
//             llvm::Value* diff_addr = builder.CreateGEP(bufferType, buffer, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(F.getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(F.getContext()), 1)}, "diff_addr");
//             builder.CreateStore(diff_time, diff_addr);
//             // we insert call time_logger(buffer, buffer_idx) before the return instruction
//             llvm::Function* time_logger = F.getParent()->getFunction("time_logger");
//             if (!time_logger) {
//                 time_logger = createTimeLogger(*F.getParent());
//             }
//             builder.CreateCall(time_logger, {buffer, buffer_idx});
//         }
//     }
// }

