#include "model/CFAModel.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Casting.h"

namespace uppllvm {

static uint32_t nextLocId = 0;
static uint32_t bbId = 0;
// static uint32_t callId = 0;

// Reset static counters to start from 0
void resetCFABuilderCounters() {
    nextLocId = 0;
    bbId = 0;
}

static void SplitCallSitesInBlock(llvm::BasicBlock& BB) {
    // Prepare split points array, split points should be the position of call instructions and the position of the next instruction.
    std::vector<int> splitPoints;
    int i = 0;
    std::vector<int> callSites; // target call sites
    
    for(auto& instr : BB){
        // we only split non-intrinsic call instructions
        if (auto callInst = llvm::dyn_cast<llvm::CallBase>(&instr)) {
            if (callInst->isIndirectCall()) {
                callSites.push_back(i);
            } else {
                if (auto calledFunc = callInst->getCalledFunction()) {
                    if (!calledFunc->isIntrinsic() && !calledFunc->isDeclaration()) {
                        callSites.push_back(i);
                    }
                }
            }
        }
        i++;
    }

    if(callSites.size() == 0){
        return;
    }

    // Add the position of each call instruction and the position of the next instruction as split points
    for(int i = 0; i < callSites.size(); i++){
        splitPoints.push_back(callSites[i]);
        splitPoints.push_back(callSites[i]+1);
    }

    // Split the basic block according to the split points array
    auto it = BB.begin();
    llvm::BasicBlock* toSplit = &BB;
    int currSize = BB.size();
    int last = 0; // last split point
    // uint32_t subBlockID = 0;
    
    for(int i = 0; i < splitPoints.size() ; i++){
        for(int j = 0; j < splitPoints[i] - last ; j++){
            it++;
        }
        if(splitPoints[i] == 0 || splitPoints[i] == currSize - 1){
            continue;
        }
        last = splitPoints[i];
        if(toSplit->size() <= 2)
            continue;
        toSplit = toSplit->splitBasicBlock(it);
    }
}

static void RefactorFunc(llvm::Function* F) {
    // 1. Split call sites in each basic block
    for(auto& BB : *F){
        SplitCallSitesInBlock(BB);
    }
    // 2. rename basic block
    for(auto& BB : *F){
        BB.setName("bbNum" + std::to_string(bbId++));
    }
}

void CFAModel::createCFAModel() {
    RefactorFunc(func);
    
    // Map from basic block to its location
    std::unordered_map<llvm::BasicBlock*, CFALocation*> bbToLoc;
    // Map from intermediate locations to the basic block that created them (for conditional branches)
    std::unordered_map<CFALocation*, llvm::BasicBlock*> interToSourceBB;
    
    // Step 1: Create entry and exit locations
    llvm::BasicBlock* entryBB = &func->getEntryBlock();
    CFALocation* entryLoc = createLocation(true);
    entryLoc->setBB(entryBB);
    bbToLoc[entryBB] = entryLoc;
    
    // Create exit location
    CFALocation* exitLoc = createLocation(false);
    exitLoc->setBB(nullptr);  // Exit location doesn't correspond to any specific basic block
    setExitLocation(exitLoc);
    
    // Step 2: Create locations for all other basic blocks
    for (auto& BB : *func) {
        if (&BB != entryBB) {
            CFALocation* loc = createLocation(false);
            loc->setBB(&BB);
            bbToLoc[&BB] = loc;
        }
    }
    
    // Step 3: Process each basic block and create edges
    for (auto& BB : *func) {
        CFALocation* currLoc = bbToLoc[&BB];
        
        // Collect non-terminator, non-PHI instructions
        std::vector<llvm::Instruction*> nonTermInstrs;
        for (auto& inst : BB) {
            if (inst.isTerminator()) {
                break;
            }
            // Skip PHI instructions
            if (llvm::isa<llvm::PHINode>(&inst)) {
                continue;
            }
            nonTermInstrs.push_back(&inst);
        }
        
        // Check if this block only contains call instructions (after RefactorFunc splitting)
        // Exclude blocks that only contain debug intrinsic calls
        bool onlyCallInstrs = !nonTermInstrs.empty();
        int debugCallCount = 0;
        for (auto* inst : nonTermInstrs) {
            if (!llvm::isa<llvm::CallBase>(inst)) {
                onlyCallInstrs = false;
                break;
            }
            if (llvm::isa<llvm::DbgInfoIntrinsic>(inst)) {
                debugCallCount++;
            }
        }
        // If all instructions are debug calls, don't treat as CALL edge
        if (debugCallCount == nonTermInstrs.size()) {
            onlyCallInstrs = false;
        }
        
        // Get terminator
        llvm::Instruction* terminator = BB.getTerminator();
        
        // Handle unconditional branch
        if (auto* br = llvm::dyn_cast<llvm::BranchInst>(terminator)) {
            if (br->isUnconditional()) {
                llvm::BasicBlock* targetBB = br->getSuccessor(0);
                CFALocation* targetLoc = bbToLoc[targetBB];
                EdgeType edgeType = onlyCallInstrs ? EdgeType::CALL : EdgeType::SEQUENTIAL;
                CFAEdge* edge = createEdge(currLoc, targetLoc, edgeType);
                
                // Add non-terminator instructions to edge
                for (auto* inst : nonTermInstrs) {
                    edge->addLLVMInstruction(inst);
                }
            }
            // Handle conditional branch
            else {
                // Create an intermediate location for the branch
                CFALocation* branchLoc = createLocation(false);
                branchLoc->setBB(nullptr);  // Intermediate location should not have a basic block
                // Track that this intermediate location was created by the current basic block
                interToSourceBB[branchLoc] = &BB;
                EdgeType edgeType = onlyCallInstrs ? EdgeType::CALL : EdgeType::SEQUENTIAL;
                CFAEdge* preBranchEdge = createEdge(currLoc, branchLoc, edgeType);
                
                // Add non-terminator instructions to pre-branch edge
                for (auto* inst : nonTermInstrs) {
                    preBranchEdge->addLLVMInstruction(inst);
                }
                
                // Get the condition variable from the branch instruction and save it to the pre-branch edge
                llvm::Value* condBrVar = br->getCondition();
                preBranchEdge->setCondBrVar(condBrVar);
                
                // Create true and false branches
                llvm::BasicBlock* trueBB = br->getSuccessor(0);
                llvm::BasicBlock* falseBB = br->getSuccessor(1);
                CFALocation* trueLoc = bbToLoc[trueBB];
                CFALocation* falseLoc = bbToLoc[falseBB];
                
                CFAEdge* trueEdge = createEdge(branchLoc, trueLoc, EdgeType::BRANCH_TRUE);
                // Note: terminator instruction is not added to branch edges as per design
                
                CFAEdge* falseEdge = createEdge(branchLoc, falseLoc, EdgeType::BRANCH_FALSE);
                // Note: terminator instruction is not added to branch edges as per design
            }
        }
        // Handle return instruction
        else if (llvm::isa<llvm::ReturnInst>(terminator)) {
            // According to the new design: L1 -[instrs]-> L2 -[return]-> exit
            // If there are non-terminator instructions, create an intermediate location
            if (!nonTermInstrs.empty()) {
                // Create intermediate location for non-terminator instructions
                CFALocation* intermediateLoc = createLocation(false);
                intermediateLoc->setBB(nullptr);  // Intermediate location should not have a basic block
                
                // Create edge from current location to intermediate location with non-terminator instructions
                EdgeType preReturnEdgeType = onlyCallInstrs ? EdgeType::CALL : EdgeType::SEQUENTIAL;
                CFAEdge* preReturnEdge = createEdge(currLoc, intermediateLoc, preReturnEdgeType);
                
                // Add non-terminator instructions to pre-return edge
                for (auto* inst : nonTermInstrs) {
                    preReturnEdge->addLLVMInstruction(inst);
                }
                
                // Create RETURN edge from intermediate location to exit location
                CFAEdge* returnEdge = createEdge(intermediateLoc, exitLocation, EdgeType::RETURN);
                // Add the return instruction to the RETURN edge
                returnEdge->addLLVMInstruction(terminator);
            } else {
                // No non-terminator instructions, create RETURN edge directly from current location to exit
                CFAEdge* returnEdge = createEdge(currLoc, exitLocation, EdgeType::RETURN);
                // Add the return instruction to the RETURN edge
                returnEdge->addLLVMInstruction(terminator);
            }
        }
        // Handle unreachable instruction
        else if (llvm::isa<llvm::UnreachableInst>(terminator)) {
            // Create edge to exit location and add all instructions (including unreachable) to it
            EdgeType edgeType = onlyCallInstrs ? EdgeType::CALL : EdgeType::SEQUENTIAL;
            CFAEdge* unreachableEdge = createEdge(currLoc, exitLocation, edgeType);
            
            // Add non-terminator instructions to edge
            for (auto* inst : nonTermInstrs) {
                unreachableEdge->addLLVMInstruction(inst);
            }
            
            // Add the unreachable instruction to the edge
            unreachableEdge->addLLVMInstruction(terminator);
        }
        // Other terminators (switch, indirect branch, etc.)
        else {
            LLVM_ERROR("Terminator is not supported: " << *terminator);
            llvm_unreachable("CFABuilder::createCFAModel: other terminators are not supported");
        }
    }
    // bool test = false;
    // Step 4: Handle PHI instructions by expanding them on incoming edges
    for (auto& BB : *func) {
        CFALocation* currLoc = bbToLoc[&BB];
        
        // Process PHI nodes in this block
        for (auto& inst : BB) {
            if (auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst)) {
                // For each incoming value of the PHI
                for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
                    llvm::BasicBlock* incomingBB = phi->getIncomingBlock(i);
                    llvm::Value* incomingValue = phi->getIncomingValue(i);
                    
                    // Find incoming edges to currLoc and check if they come from incomingBB
                    bool foundMatchingEdge = false;
                    for (CFAEdge* edge : currLoc->getIncomingEdges()) {
                        CFALocation* edgeSrc = edge->getSrcLoc();
                        llvm::BasicBlock* edgeSrcBB = edgeSrc->getBB();
                        
                        // Check if this edge comes from incomingBB
                        // If edgeSrc is an intermediate location (BB is nullptr), check interToSourceBB
                        llvm::BasicBlock* actualSourceBB = edgeSrcBB;
                        if (edgeSrcBB == nullptr) {
                            // This is an intermediate location, find its source basic block
                            auto it = interToSourceBB.find(edgeSrc);
                            if (it != interToSourceBB.end()) {
                                actualSourceBB = it->second;
                            }
                        }
                        
                        bool isFromIncomingBB = (actualSourceBB == incomingBB);
                        
                        if (isFromIncomingBB) {
                            // Check if this is a CALL edge
                            if (edge->getType() == EdgeType::CALL) {
                                // Create intermediate location to separate call and PHI assignment
                                CFALocation* intermediateLoc = createLocation(false);
                                intermediateLoc->setBB(nullptr);  // Intermediate location
                                
                                // Change the call edge's destination to intermediate location
                                CFALocation* originalDst = edge->getDstLoc();
                                edge->changeDstLoc(intermediateLoc);
                                
                                // Create new edge from intermediate to original destination for PHI assignment
                                CFAEdge* phiEdge = createEdge(intermediateLoc, originalDst, EdgeType::SEQUENTIAL);
                                phiEdge->addAssignmentInstruction(phi, incomingValue);
                                // test = true;
                            } else {
                                // Add PHI assignment to this edge directly
                                edge->addAssignmentInstruction(phi, incomingValue);
                            }
                            foundMatchingEdge = true;
                            break;
                        }
                    }
                    
                    // Check if we found a matching edge
                    if (!foundMatchingEdge) {
                        LLVM_ERROR("PHI instruction references incoming basic block " 
                              << incomingBB->getName() 
                              << " but no corresponding edge found in CFA for block " 
                              << BB.getName() << " - PHI: " << *phi);
                        llvm_unreachable("CFABuilder::createCFAModel: Missing edge for PHI instruction");
                    }
                }
            } else {
                // PHI nodes always come first, so we can break here
                break;
            }
        }
    }
    // if (test){
    //     this->ToDot("CFAModel_after_phi.dot");
    // }
}

CFALocation* CFAModel::createLocation(bool isEntry) {
    auto location = std::make_unique<CFALocation>(nextLocId++);
    CFALocation* locPtr = location.get();
    uint32_t locId = locPtr->getLocId();
    locations[locId] = std::move(location);
    if (isEntry) {
        entryLocation = locPtr;
    }
    return locPtr;
}

CFAEdge* CFAModel::createEdge(CFALocation* srcLoc, CFALocation* dstLoc, EdgeType type) {
    if (!srcLoc || !dstLoc) {
        ERROR("CFABuilder::createEdge: srcLoc or dstLoc is nullptr");
        exit(1);
    }
    // Create new edge
    auto edge = std::make_unique<CFAEdge>(srcLoc, dstLoc, type);
    CFAEdge* edgePtr = edge.get();
    // Store in containers
    edges.push_back(std::move(edge));
    return edgePtr;
}

void CFAModel::removeEdge(CFAEdge* edge) {
    if (!edge) {
        return;
    }
    // Remove edge from source location's outgoing edges
    CFALocation* srcLoc = edge->getSrcLoc();
    if (srcLoc) {
        srcLoc->removeOutgoingEdge(edge);
    }
    // Remove edge from destination location's incoming edges
    CFALocation* dstLoc = edge->getDstLoc();
    if (dstLoc) {
        dstLoc->removeIncomingEdge(edge);
    }
    
    // Remove edge from the model's edge collection
    auto it = std::find_if(edges.begin(), edges.end(),
        [edge](const std::unique_ptr<CFAEdge>& e) {
            return e.get() == edge;
        });
    
    if (it != edges.end()) {
        edges.erase(it);
    }
}

void CFAModel::removeEdges(CFALocation* src, CFALocation* dst) {
    if (!src || !dst) {
        return;
    }
    
    // Find all edges from src to dst and remove them
    auto it = edges.begin();
    while (it != edges.end()) {
        CFAEdge* edge = it->get();
        if (edge->getSrcLoc() == src && edge->getDstLoc() == dst) {
            // Remove from source and destination locations
            src->removeOutgoingEdge(edge);
            dst->removeIncomingEdge(edge);
            // Remove from edges collection
            it = edges.erase(it);
        } else {
            ++it;
        }
    }
}

void CFAModel::removeLocation(uint32_t locId) {
    auto it = locations.find(locId);
    if (it == locations.end()) {
        return;
    }
    CFALocation* loc = it->second.get();
    removeLocation(loc);
}

void CFAModel::removeLocation(CFALocation* loc) {
    if (!loc) {
        return;
    }
    
    // First, remove all edges connected to this location
    // We need to copy the edge vectors because removing edges will modify them
    std::vector<CFAEdge*> incomingEdgesCopy = loc->getIncomingEdges();
    std::vector<CFAEdge*> outgoingEdgesCopy = loc->getOutgoingEdges();
    
    // Remove all incoming edges
    for (CFAEdge* edge : incomingEdgesCopy) {
        removeEdge(edge);
    }
    // Remove all outgoing edges
    for (CFAEdge* edge : outgoingEdgesCopy) {
        removeEdge(edge);
    }
    // If this is the entry location, clear the entry location pointer
    if (entryLocation == loc) {
        entryLocation = nullptr;
    }
    // Remove location from the model's location collection
    uint32_t locId = loc->getLocId();
    auto it = locations.find(locId);
    if (it != locations.end()) {
        locations.erase(it);
    }
}

void CFAModel::ToDot(const std::string& filename) const {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        ERROR("Failed to open dot file: " + filename);
        return;
    }
    ofs << "digraph CFA {\n";
    ofs << "  rankdir=TD;\n";
    ofs << "  node [shape=box, fontsize=10];\n";

    // Emit nodes with id; basic block name
    for (const auto& [locId, locPtr] : locations) {
        CFALocation* loc = locPtr.get();
        std::string label = std::to_string(loc->getLocId());
        // Only add basic block name if the location has an associated basic block
        if (loc->getBB()) {
            label = label + " (" + loc->getBB()->getName().str() + ")";
        }
        // If no BB, just use the location ID without additional label
        ofs << "  L" << loc->getLocId() << " [label=\"" << label << "\"];\n";
    }

    // Helper: stringify CFAInstruction for edge labels
    auto stringifyInstruction = [](const CFAInstruction& inst) -> std::string {
        std::string s;
        if (inst.getType() == CFAInstruction::LLVM_IR_INSTRUCTION) {
            if (auto* ir = inst.getLLVMInstr()) {
                std::string str;
                llvm::raw_string_ostream rso(str);
                ir->print(rso);
                s = rso.str();
            }
        } else if (inst.getType() == CFAInstruction::EXPAND_ASSIGNMENT) {
            // 处理自定义指令
            const auto& assignInfo = inst.getAssign();
            if (assignInfo && assignInfo->formalArg && assignInfo->actualArg) {
                std::string lhs, rhs;
                
                // For lhs, extract variable name
                std::string tmp1; 
                llvm::raw_string_ostream rso1(tmp1);
                assignInfo->formalArg->printAsOperand(rso1, false);
                lhs = rso1.str();
                
                // For rhs, extract value  
                std::string tmp2; 
                llvm::raw_string_ostream rso2(tmp2);
                assignInfo->actualArg->printAsOperand(rso2, false);
                rhs = rso2.str();
                
                // Create assignment string
                s = lhs + " = " + rhs;
            }
            // 可以在此添加其他自定义指令类型的字符串化
        }
        // compact label: escape quotes and newlines
        for (char& c : s) {
            if (c == '"') c = '\''; // rough escape
            if (c == '\n' || c == '\r') c = ' ';
        }
        return s;
    };

    // Emit edges with ordered instruction stream
    for (const auto& edgeUP : edges) {
        const CFAEdge* e = edgeUP.get();
        CFALocation* src = e->getSrcLoc();
        CFALocation* dst = e->getDstLoc();
        std::string label;
        const auto& insts = e->getInstructions();
        for (size_t i = 0; i < insts.size(); ++i) {
            if (i) label += "\n";
            label += stringifyInstruction(insts[i]);
        }
        // Add condition branch variable if present
        if (e->hasCondBrVar()) {
            if (!label.empty()) {
                label += "\n";
            }
            std::string condStr;
            llvm::raw_string_ostream rso(condStr);
            e->getCondBrVar()->printAsOperand(rso, false);
            label += "<br cond " + rso.str() + ">";
        }
        // Add True/False labels for branch edges
        if (e->getType() == EdgeType::BRANCH_TRUE) {
            if (!label.empty()) {
                label += "\n";
            }
            label += "True";
        } else if (e->getType() == EdgeType::BRANCH_FALSE) {
            if (!label.empty()) {
                label += "\n";
            }
            label += "False";
        }
        ofs << "  L" << src->getLocId() << " -> L" << dst->getLocId();
        if (!label.empty()) {
            ofs << " [label=\"" << label << "\", fontsize=9]";
        }
        ofs << ";\n";
    }

    ofs << "}\n";
    ofs.close();
}

} // namespace uppllvm
