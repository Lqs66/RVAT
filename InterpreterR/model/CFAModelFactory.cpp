#include "model/CFAModelFactory.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/WithColor.h"
#include "model/CFAModel.h"
#include "macro.hh"
#include <algorithm>
#include <exception>
#include <queue>
#include <set>
#include <map>

namespace uppllvm {
struct CallEdgeInfo {
    uppllvm::CFAEdge* edge;
    uint32_t depth;
    llvm::Function* calledFunc;
    llvm::CallBase* callInst;
    CallEdgeInfo(uppllvm::CFAEdge* edge, uint32_t depth, llvm::Function* calledFunc, llvm::CallBase* callInst) 
        : edge(edge), depth(depth), calledFunc(calledFunc), callInst(callInst) {}
};

// Forward declaration of reset function from CFABuilder.cpp
void resetCFABuilderCounters();
}

bool CFAModelFactory::createCFAModels(
    llvm::Module* module,
    const std::vector<std::string>& targetFuncsName,
    const std::string& entryPointFuncName,
    std::size_t call_depth,
    std::vector<std::unique_ptr<uppllvm::CFAModel>>& cfaModels,
    uppllvm::CFAModel*& entryPointCFAModel,
    bool merge)
{
    // Reset static counters to ensure IDs start from 0
    uppllvm::resetCFABuilderCounters();
    
    if (!module) {
        ERROR("Module is not initialized");
        return false;
    }

    // Clear existing models
    cfaModels.clear();
    entryPointCFAModel = nullptr;

    // Get entry function
    llvm::Function* entryFunc = module->getFunction(entryPointFuncName);
    if (!entryFunc) {
        ERROR("Entry function not found");
        return false;
    }

    // Step 1: Traverse call graph starting from entry function
    // Using BFS to collect all functions within call_depth
    std::map<std::string, llvm::Function*> collectedFuncsMap; // Use map for deterministic ordering by function name
    std::queue<std::pair<llvm::Function*, std::size_t>> funcQueue; // <function, depth>
    std::map<llvm::Function*, std::size_t> funcDepthMap; // Track depth for each function

    // Start from entry function at depth 0
    funcQueue.push({entryFunc, 0});
    collectedFuncsMap[entryFunc->getName().str()] = entryFunc;
    funcDepthMap[entryFunc] = 0;

    while (!funcQueue.empty()) {
        auto [currentFunc, currentDepth] = funcQueue.front();
        funcQueue.pop();

        // Skip if we've reached maximum depth
        if (currentDepth >= call_depth) {
            continue;
        }

        // Skip declaration-only functions (they have no body)
        if (currentFunc->isDeclaration()) {
            continue;
        }

        // Traverse all basic blocks and instructions in current function
        for (llvm::BasicBlock& BB : *currentFunc) {
            for (llvm::Instruction& I : BB) {
                // Check if this is a call instruction (CallInst or InvokeInst)
                if (auto* callBase = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    // Skip indirect calls (we only consider direct calls)
                    if (callBase->isIndirectCall()) {
                        continue;
                    }
                    // Get the called function (only works for direct calls)
                    llvm::Function* calledFunc = callBase->getCalledFunction();
                    if (!calledFunc) {
                        continue;
                    }
                    // Skip intrinsic functions and declarations
                    if (calledFunc->isIntrinsic() || calledFunc->isDeclaration()) {
                        continue;
                    }
                    // If we haven't collected this function yet, add it
                    std::string calledFuncName = calledFunc->getName().str();
                    if (collectedFuncsMap.find(calledFuncName) == collectedFuncsMap.end()) {
                        std::size_t nextDepth = currentDepth + 1;
                        collectedFuncsMap[calledFuncName] = calledFunc;
                        funcDepthMap[calledFunc] = nextDepth;
                        
                        // Only add to queue if within depth limit
                        if (nextDepth < call_depth) {
                            funcQueue.push({calledFunc, nextDepth});
                        }
                    }
                }
            }
        }
    }

    // We now have the map of functions collected from call graph traversal
    // The collectedFuncsMap contains all functions reachable from entry function within call_depth
    // Sorted by function name for deterministic ordering
    // Step 2: Create CFAModels for the collected functions
    std::map<llvm::Function*, uppllvm::CFAModel*> cfaModelMap;
    for (const auto& [funcName, func] : collectedFuncsMap) {
        auto cfaModel = std::make_unique<uppllvm::CFAModel>(func);
        cfaModelMap[func] = cfaModel.get();
        cfaModels.push_back(std::move(cfaModel));
    }
    entryPointCFAModel = cfaModelMap[entryFunc];

    // Check if there are isolated locations in CFAModel, output ToDot() if there are
    // for (const auto& [func, cfaModel] : cfaModelMap) {
    //     for (const auto& location : cfaModel->getLocations()) {
    //         if (!location->hasIncomingEdges() && !location->hasOutgoingEdges()) {
    //             std::string dotFileName = cfaModel->getFunc()->getName().str() + ".dot";
    //             cfaModel->ToDot(dotFileName);
    //             LLVM_INFO("Isolated location found in " << cfaModel->getFunc()->getName() << ", output ToDot() to " << dotFileName);
    //             break;
    //         }
    //     }
    // }

    // Step 3: Merge CFAModels to entryPointCFAModel if needed
    if (merge) {
        INFO("Merging CFAModels...");
        std::queue<uppllvm::CallEdgeInfo> edgesToMerge;
        for (uppllvm::CFAEdge* edge : entryPointCFAModel->getEdges()) {
            if (edge->getType() == uppllvm::EdgeType::CALL) {
                if (edge->getInstructions().size() != 1) {
                    ERROR("Call edge has " << edge->getInstructions().size() << " instructions, expected 1");
                    for (const auto& inst : edge->getInstructions()) {
                        if (inst.getType() == uppllvm::CFAInstruction::LLVM_IR_INSTRUCTION) {
                            llvm::Instruction* llvmInst = inst.getLLVMInstr();
                            llvm::WithColor::error(llvm::errs(), "CFAModelFactory")
                                << "Instruction: " << *llvmInst << "\n";
                        } else if (inst.getType() == uppllvm::CFAInstruction::EXPAND_ASSIGNMENT) {
                            auto& assign = inst.getAssign();
                            llvm::WithColor::error(llvm::errs(), "CFAModelFactory")
                                << "Assignment Instruction: \nformalArg=" << *(assign->formalArg)
                                << "\nactualArg=" << *(assign->actualArg) << "\n";
                        }
                    }
                    abort();
                }
                // Get the called function from the call instruction
                llvm::CallBase* callBase = nullptr;
                for (const auto& inst : edge->getInstructions()) {
                    if (inst.getType() == uppllvm::CFAInstruction::LLVM_IR_INSTRUCTION) {
                        if (auto* cb = llvm::dyn_cast<llvm::CallBase>(inst.getLLVMInstr())) {
                            callBase = cb;
                            break;
                        }
                    }
                }
                if (!callBase || callBase->isIndirectCall()) {
                    continue;
                }
                llvm::Function* calledFunc = callBase->getCalledFunction();
                if (!calledFunc || calledFunc->isIntrinsic() || calledFunc->isDeclaration()) {
                    continue;
                }
                edgesToMerge.push({edge, 0, calledFunc, callBase});
            }
        }
        while (!edgesToMerge.empty()) {
            uppllvm::CallEdgeInfo callEdgeInfo = edgesToMerge.front();
            edgesToMerge.pop();
            // Skip if we've reached maximum depth
            if (callEdgeInfo.depth >= call_depth) {
                continue;
            }
            uppllvm::CFAModel* calledCFAModel = cfaModelMap[callEdgeInfo.calledFunc];
            if (!calledCFAModel) {
                continue;
            }
            
            // Get call edge's source and destination locations
            uppllvm::CFAEdge* callEdge = callEdgeInfo.edge;
            uppllvm::CFALocation* callSrcLoc = callEdge->getSrcLoc();
            uppllvm::CFALocation* callDstLoc = callEdge->getDstLoc();
            
            // Get call instruction
            llvm::CallBase* callInst = callEdgeInfo.callInst;
            if (!callInst) {
                continue;
            }
            
            // Create location mapping table: callee location -> new location in entryPointCFAModel
            std::map<uppllvm::CFALocation*, uppllvm::CFALocation*> locMapping;
            
            // Copy all locations from called function
            for (uppllvm::CFALocation* origLoc : calledCFAModel->getLocations()) {
                uppllvm::CFALocation* newLoc = entryPointCFAModel->createLocation();
                newLoc->setBB(origLoc->getBB());
                locMapping[origLoc] = newLoc;
            }
            
            // Get called function's entry and exit locations (mapped)
            uppllvm::CFALocation* calleeEntryLoc = calledCFAModel->getEntryLocation();
            uppllvm::CFALocation* calleeExitLoc = calledCFAModel->getExitLocation();
            uppllvm::CFALocation* newCalleeEntry = locMapping[calleeEntryLoc];
            uppllvm::CFALocation* newCalleeExit = locMapping[calleeExitLoc];
            
            // Temporary storage for new edges, used to find new calls
            std::vector<uppllvm::CFAEdge*> newEdges;
            
            // Copy all edges from called function
            for (uppllvm::CFAEdge* origEdge : calledCFAModel->getEdges()) {
                uppllvm::CFALocation* newSrcLoc = locMapping[origEdge->getSrcLoc()];
                uppllvm::CFALocation* newDstLoc = locMapping[origEdge->getDstLoc()];
                
                // Create new edge in entryPointCFAModel
                uppllvm::CFAEdge* newEdge = entryPointCFAModel->createEdge(newSrcLoc, newDstLoc, origEdge->getType());
                
                // Copy instructions on the edge
                for (const auto& inst : origEdge->getInstructions()) {
                    if (inst.getType() == uppllvm::CFAInstruction::LLVM_IR_INSTRUCTION) {
                        newEdge->addLLVMInstruction(inst.getLLVMInstr());
                    } else if (inst.getType() == uppllvm::CFAInstruction::EXPAND_ASSIGNMENT) {
                        auto& assign = inst.getAssign();
                        newEdge->addAssignmentInstruction(
                            assign->formalArg, assign->actualArg);
                    }
                }
                
                // Copy conditional branch variable
                if (origEdge->hasCondBrVar()) {
                    newEdge->setCondBrVar(origEdge->getCondBrVar());
                }
                
                newEdges.push_back(newEdge);
            }
            
            // Handle return edges: replace RETURN edges with edges connecting to post_call location
            std::set<uppllvm::CFAEdge*> removedEdges;
            for (uppllvm::CFAEdge* newEdge : newEdges) {
                if (newEdge->getType() == uppllvm::EdgeType::RETURN && 
                    newEdge->getDstLoc() == newCalleeExit) {
                    // This is a return edge to exit, need to connect to post_call location
                    uppllvm::CFALocation* returnSrcLoc = newEdge->getSrcLoc();
                    
                    // Create new return value assignment edge (from return source location to post_call location)
                    uppllvm::CFAEdge* returnEdge = entryPointCFAModel->createEdge(
                        returnSrcLoc, callDstLoc, uppllvm::EdgeType::SEQUENTIAL);
                    
                    // Fix: If callDstLoc is a branch point, copy condBrVar from preBranchEdge
                    // This ensures evaluateGuard is called on all paths to branch points
                    if (callDstLoc->getOutgoingEdges().size() == 2) {
                        bool hasBranchEdges = false;
                        for (auto* outEdge : callDstLoc->getOutgoingEdges()) {
                            if (outEdge->getType() == uppllvm::EdgeType::BRANCH_TRUE || 
                                outEdge->getType() == uppllvm::EdgeType::BRANCH_FALSE) {
                                hasBranchEdges = true;
                                break;
                            }
                        }
                        if (hasBranchEdges) {
                            // Find the preBranchEdge with condBrVar and copy it to returnEdge
                            for (auto* inEdge : callDstLoc->getIncomingEdges()) {
                                if (inEdge->hasCondBrVar() && inEdge != returnEdge) {
                                    returnEdge->setCondBrVar(inEdge->getCondBrVar());
                                    break;
                                }
                            }
                        }
                    }
                    
                    // Copy return instructions (if there is a return value)
                    for (const auto& inst : newEdge->getInstructions()) {
                        if (inst.getType() == uppllvm::CFAInstruction::LLVM_IR_INSTRUCTION) {
                            llvm::Instruction* llvmInst = inst.getLLVMInstr();
                            if (llvm::isa<llvm::ReturnInst>(llvmInst)) {
                                llvm::ReturnInst* retInst = llvm::cast<llvm::ReturnInst>(llvmInst);
                                if (!retInst->getReturnValue()) {
                                    // Don't need to assign return value
                                    // returnEdge->addLLVMInstruction(llvmInst);
                                } else {
                                    // There is a return value, need to assign to the result variable at call point
                                    llvm::Value* returnValue = retInst->getReturnValue();
                                    llvm::Value* resultVar = callInst; // The value of call instruction is the result variable
                                    returnEdge->addAssignmentInstruction(resultVar, returnValue);
                                }
                            } else {
                                returnEdge->addLLVMInstruction(llvmInst);
                            }
                        } else if (inst.getType() == uppllvm::CFAInstruction::EXPAND_ASSIGNMENT) {
                            auto& assign = inst.getAssign();
                            returnEdge->addAssignmentInstruction(
                                assign->formalArg, assign->actualArg);
                        }
                    }
                    
                    // Mark as deleted, later excluded from newEdges
                    removedEdges.insert(newEdge);
                    // Delete original return edge
                    entryPointCFAModel->removeEdge(newEdge);
                }
            }
            
            // After reconnecting RETURN edges, remove the callee's exit location if it is now isolated
            if (newCalleeExit && !newCalleeExit->hasIncomingEdges() && !newCalleeExit->hasOutgoingEdges()) {
                entryPointCFAModel->removeLocation(newCalleeExit);
            }
            
            // Create parameter assignment edge: from call location to called function's entry
            uppllvm::CFAEdge* paramEdge = entryPointCFAModel->createEdge(
                callSrcLoc, newCalleeEntry, uppllvm::EdgeType::SEQUENTIAL);
            
            // Add parameter assignment instructions
            llvm::Function* calledFunc = callEdgeInfo.calledFunc;
            for (size_t i = 0; i < calledFunc->arg_size() && i < callInst->arg_size(); ++i) {
                llvm::Argument* formalArg = calledFunc->getArg(i);
                llvm::Value* actualArg = callInst->getArgOperand(i);
                paramEdge->addAssignmentInstruction(formalArg, actualArg);
            }
            
            // Delete original call edge
            entryPointCFAModel->removeEdge(callEdge);
            
            // Find new call edges and add to queue (skip deleted edges)
            for (uppllvm::CFAEdge* newEdge : newEdges) {
                if (removedEdges.find(newEdge) != removedEdges.end()) {
                    continue; // Skip deleted edges
                }
                if (newEdge->getType() == uppllvm::EdgeType::CALL) {
                    // Check if the edge has a call instruction
                    for (const auto& inst : newEdge->getInstructions()) {
                        if (inst.getType() == uppllvm::CFAInstruction::LLVM_IR_INSTRUCTION) {
                            if (auto* cb = llvm::dyn_cast<llvm::CallBase>(inst.getLLVMInstr())) {
                                if (!cb->isIndirectCall()) {
                                    llvm::Function* nestedCalledFunc = cb->getCalledFunction();
                                    if (nestedCalledFunc && !nestedCalledFunc->isIntrinsic() && 
                                        !nestedCalledFunc->isDeclaration()) {
                                        // Find new call edges and add to queue (include call instruction)
                                        edgesToMerge.push({newEdge, callEdgeInfo.depth + 1, nestedCalledFunc, cb});
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}


#ifdef DEBUG
bool CFAModelFactory::createCFAModelsForTest(
    llvm::Module* module,
    const std::vector<std::string>& targetFuncsName,
    const std::string& entryPointFuncName,
    std::vector<std::unique_ptr<uppllvm::CFAModel>>& cfaModels,
    uppllvm::CFAModel*& entryPointCFAModel)
{
    if (!module) {
        ERROR("Module is not initialized");
        return false;
    }

    // Clear existing models
    cfaModels.clear();
    entryPointCFAModel = nullptr;

    // Check if entry point is in target functions list
    bool entryPointInTargets = std::find(targetFuncsName.begin(), targetFuncsName.end(), 
                                         entryPointFuncName) != targetFuncsName.end();

    // Create CFAModels for target functions
    for (const auto& funcName : targetFuncsName) {
        llvm::Function* func = module->getFunction(funcName);
        if (!func) {
            llvm::WithColor::warning(llvm::errs(), "CFAModelFactory")
                << "Function '" << funcName << "' not found in module. Skipping...\n";
            continue;
        }

        try {
            auto cfaModel = std::make_unique<uppllvm::CFAModel>(func);
            
            // If this is the entry point, also set the entryPointCFAModel pointer
            if (funcName == entryPointFuncName) {
                entryPointCFAModel = cfaModel.get();
            }
            
            cfaModels.push_back(std::move(cfaModel));
        } catch (const std::exception& e) {
            llvm::WithColor::error(llvm::errs(), "CFAModelFactory")
                << "Failed to create CFAModel for function '" << funcName 
                << "': " << e.what() << "\n";
            return false;
        }
    }

    // Create CFAModel for entry point function if it's not in target functions
    if (!entryPointInTargets) {
        llvm::Function* entryFunc = module->getFunction(entryPointFuncName);
        if (!entryFunc) {
            llvm::WithColor::error(llvm::errs(), "CFAModelFactory")
                << "Entry point function '" << entryPointFuncName << "' not found in module.\n";
            return false;
        }

        try {
            auto entryCFAModel = std::make_unique<uppllvm::CFAModel>(entryFunc);
            entryPointCFAModel = entryCFAModel.get();
            // Note: entry point CFAModel is also added to cfaModels
            cfaModels.push_back(std::move(entryCFAModel));
        } catch (const std::exception& e) {
            llvm::WithColor::error(llvm::errs(), "CFAModelFactory")
                << "Failed to create CFAModel for entry point function '" << entryPointFuncName 
                << "': " << e.what() << "\n";
            return false;
        }
    }

    // Verify entry point CFAModel was created
    if (!entryPointCFAModel) {
        llvm::WithColor::error(llvm::errs(), "CFAModelFactory")
            << "Entry point CFAModel was not created.\n";
        return false;
    }

    return true;
}
#endif