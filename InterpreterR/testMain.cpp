#include "base/base.h"
#include "model/CFAModel.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/IR/DataLayout.h"
#include "Interpreter/InstrsInterpreter.h"
#include <iterator>
#include <functional>
#include <iostream>
#include <string>
#include <cstdlib>  // For setenv

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

// bool test_query_result() {
//     // RC_valid == true --> (manual_control_update_time<=COM_RC_LOSS_T and manual_control_loss_failsafe == false)
//     bool RC_valid_is_true = is_true("RC_valid");
//     if (RC_valid_is_true) {
//         bool manual_control_update_time_is_leq_COM_RC_LOSS_T = is_less_or_equal("manual_control_update_time", "COM_RC_LOSS_T");
//         bool manual_control_loss_failsafe_is_false = is_false("manual_control_loss_failsafe");
//         if (manual_control_update_time_is_leq_COM_RC_LOSS_T && manual_control_loss_failsafe_is_false) {
//             return true;
//         } else {
//             return false;
//         }
//     } else {
//         return true; // Implication is true when antecedent is false
//     }
// }

// Test function to execute CFA model following a single path
// This function handles conditional branches by evaluating guards and selecting the appropriate branch
void test_executeCFAModel() {
    int initId = getInitLocationID();
    int curr_loc = initId;
    std::cout << "Starting CFA model execution from location " << initId << std::endl;
    bool isTrue = true;
    // Execute until reaching a location with no outgoing edges
    while (curr_loc >= 0) {
        std::cout << "Current location: " << curr_loc << std::endl;
        if (nbOfOutgoingEdges(curr_loc)) {
            int num_edges = nbOfOutgoingEdges(curr_loc);
            if (num_edges == 1) {
                executeEdge(curr_loc, 0);
                // showQueries();
                // llvm::outs() << "query result: " << (test_query_result() ? "true" : "false") << "\n";
                if (hasGuard(curr_loc, 0)) {
                    int guard_value = evaluateGuard(curr_loc, 0);
                    // llvm::outs() << "guard_value: " << guard_value << "\n";
                    isTrue = guard_value ? true : false;
                    // llvm::outs() << "isTrue: " << isTrue << "\n";
                }
                curr_loc = getSuccLocID(curr_loc, 0);
            }else if(num_edges == 2) {
                int edge_type1 = getEdgeType(curr_loc, 0);
                int edge_type2 = getEdgeType(curr_loc, 1);
                // llvm::outs() << "edge_type1: " << edge_type1 << "\n";
                // llvm::outs() << "edge_type2: " << edge_type2 << "\n";
                // llvm::outs() << "isTrue: " << isTrue << "\n";
                if (isTrue) {
                    if (edge_type1 == 1) {
                        executeEdge(curr_loc, 0);
                        // showQueries();
                        // llvm::outs() << "query result: " << (test_query_result() ? "true" : "false") << "\n";
                        curr_loc = getSuccLocID(curr_loc, 0);
                    }else if(edge_type2 == 1) {
                        executeEdge(curr_loc, 1);
                        curr_loc = getSuccLocID(curr_loc, 1);
                    }
                }else{
                    if (edge_type1 == 2) {
                        executeEdge(curr_loc, 0);
                        // showQueries();
                        // llvm::outs() << "query result: " << (test_query_result() ? "true" : "false") << "\n";
                        curr_loc = getSuccLocID(curr_loc, 0);
                    }else if(edge_type2 == 2) {
                        executeEdge(curr_loc, 1);
                        // showQueries();
                        // llvm::outs() << "query result: " << (test_query_result() ? "true" : "false") << "\n";
                        curr_loc = getSuccLocID(curr_loc, 1);
                    }
                }
            }else {
                ERROR("Invalid number of outgoing edges: " << num_edges);
                abort();
            }
        } else {
            curr_loc = -1;
            std::cout << "No outgoing edges found at location " << curr_loc << std::endl;
            std::cout << "Execution completed. Final location: " << curr_loc << std::endl;
        }
    }
}

int main() {
    // Set MODEL_INPUT_PATH environment variable programmatically
    // This allows running without manually setting the environment variable
    
    std::string modelInputPath = "/home/lqs66/Desktop/modelCheckingFlightControl/verifyDataBase/model_inputs/PX_ORBIT3/PX_ORBIT3_ktest000008.mi";
    // std::string modelInputPath = "/home/lqs66/Desktop/modelCheckingFlightControl/verifyDataBase/model_inputs/PX_RCFS_P1/PX4_P17_ktest000030.mi";
    setenv("MODEL_INPUT_PATH", modelInputPath.c_str(), 1); // 1 means overwrite if exists

    initModel();
    // showMemory();
    // showEntryArgs();
    // EE_ptr->runStaticConstructorsDestructors(true);

    buildCFAModels();
    // printModule();
    test_executeCFAModel();
    // int initId = getInitLocationID();
    // executeEdge(initId, 0);
    // executeEdge(getSuccLocID(initId, 0), 0);
    
    //=========================================================================================
    // Test CFA Modeling
    //=========================================================================================
    // // 从IR文件读取module
    // llvm::LLVMContext C;
    // llvm::SMDiagnostic Err;
    
    // // 使用test目录中的example.ll文件作为示例
    // std::string IRFile = "/home/lqs66/Desktop/modelCheckingFlightControl/InterpreterR/test/test_phi_example.ll";
    // std::unique_ptr<llvm::Module> M = llvm::parseIRFile(IRFile, Err, C);
    
    // if (!M) {
    //     llvm::errs() << "Error reading IR file: " << IRFile << "\n";
    //     Err.print("testMain", llvm::errs());
    //     return 1;
    // }
    
    // // 使用 LowerIntrinsic 将 intrinsic 转换为普通的 LLVM IR
    // // 创建 DataLayout 和 IntrinsicLowering 来降低所有 intrinsic 调用
    // llvm::DataLayout DL(M.get());
    // std::unique_ptr<llvm::IntrinsicLowering> IL(new llvm::IntrinsicLowering(DL));
    
    // // 遍历所有函数和基本块，找到并降低所有的 intrinsic 调用
    // // 使用安全的迭代方式，因为 LowerIntrinsicCall 可能会修改指令列表
    // for (llvm::Function &F : *M) {
    //     for (llvm::BasicBlock &BB : F) {
    //         // 使用迭代器并小心处理修改
    //         for (auto I = BB.begin(), E = BB.end(); I != E; ) {
    //             if (auto *Call = llvm::dyn_cast<llvm::CallInst>(&*I)) {
    //                 if (Call->getCalledFunction() && 
    //                     Call->getCalledFunction()->isIntrinsic()) {
    //                     // LowerIntrinsicCall 可能会替换或删除指令，所以需要小心处理迭代器
    //                     llvm::BasicBlock::iterator NextI = std::next(I);
    //                     IL->LowerIntrinsicCall(Call);
    //                     I = NextI;
    //                     continue;
    //                 }
    //             }
    //             ++I;
    //         }
    //     }
    // }

    // // 获取main函数
    // llvm::Function* mainFunc = M->getFunction("main");
     
    // // 创建CFAModel并生成dot文件
    // uppllvm::CFAModel model(mainFunc);
    // llvm::outs() << *mainFunc << "\n";
    // model.ToDot("/home/lqs66/Desktop/modelCheckingFlightControl/InterpreterR/build/cfa_test.dot");
    
    // llvm::outs() << "Successfully created CFA model for main function from IR file: " << IRFile << "\n";
    // llvm::outs() << "CFA model saved to: /home/lqs66/Desktop/modelCheckingFlightControl/InterpreterR/build/cfa_test.dot\n";
    
    // llvm::Module* GlobalModule = M.get();

    // removeTypeTestAndAssume(*GlobalModule);
    // StripDebugInfo(*GlobalModule);

    // // Load the whole bitcode file eagerly.
    // {
    //     llvm::ExitOnError ExitOnErr(IRFile +
    //                         ": bitcode didn't read correctly: ");
    //     ExitOnErr(GlobalModule->materializeAll());
    // }

    // // Load system libraries to make external functions available (like sysconf, malloc, etc.)
    // // This is necessary for the interpreter to resolve external function symbols via libffi
    // std::string LibErr;
    // if (llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr, &LibErr)) {
    //     llvm::WithColor::warning(llvm::errs(), "initInterpreter") 
    //         << "Could not load program symbols: " << LibErr << "\n";
    //     llvm::errs() << "External function calls may fail.\n";
    // }

    // // Create the interpreter execution engine
    // std::string ErrorMsg;
    // std::unique_ptr<uppllvm::ExecutionEngine> EE(uppllvm::InstrsInterpreter::create(std::move(M), &ErrorMsg));
    // if (!EE) {
    //     if (!ErrorMsg.empty())
    //         llvm::WithColor::error(llvm::errs())
    //         << "error creating EE: " << ErrorMsg << "\n";
    //     else
    //         llvm::WithColor::error(llvm::errs()) << "unknown error creating EE!\n";
    //     exit(1);
    // }

    // EE->runStaticConstructorsDestructors(false);

    // std::vector<uppllvm::CFAInstruction> Instructions = model.getEdges()[0]->getInstructions();
    // EE->runInstrs(Instructions);

    return 0;
}