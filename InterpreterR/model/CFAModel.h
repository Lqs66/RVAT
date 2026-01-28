#ifndef CFA_MODEL_H
#define CFA_MODEL_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include <unordered_map>
#include <vector>
#include <string>

#include "config_parse.hpp"
#include "macro.hh"
#include "user_utils.hh"

namespace uppllvm {

class CFALocation;
class CFAEdge;
// Forward declaration for edge type used in signatures below
enum class EdgeType;

class CFAInstruction {
public:
    struct AssignmentInfo {
        llvm::Value* formalArg;
        llvm::Value* actualArg;
        
        AssignmentInfo(llvm::Value* formal, llvm::Value* actual) 
            : formalArg(formal), actualArg(actual) {}

        void print() const {
            if (actualArg && formalArg) {
                llvm::outs() << *formalArg << " = " << *actualArg << "\n";
            } else {
                llvm::outs() << "Invalid assignment (null pointer)\n";
            }
        }
    };
    enum InstructionType {
        LLVM_IR_INSTRUCTION,    // LLVM IR instruction
        EXPAND_ASSIGNMENT       // expand assignment instruction
    };

    CFAInstruction(llvm::Instruction* llvmInstr)
        : type(LLVM_IR_INSTRUCTION), llvmInstr(llvmInstr) {}

    CFAInstruction(llvm::Value* formalArg, llvm::Value* actualArg)
        : type(EXPAND_ASSIGNMENT), llvmInstr(nullptr), Assign(std::make_unique<AssignmentInfo>(formalArg, actualArg)) {}

    // Copy constructor
    CFAInstruction(const CFAInstruction& other)
        : type(other.type), llvmInstr(other.llvmInstr) {
        if (other.Assign) {
            Assign = std::make_unique<AssignmentInfo>(other.Assign->formalArg, other.Assign->actualArg);
        }
    }

    // Copy assignment operator
    CFAInstruction& operator=(const CFAInstruction& other) {
        if (this != &other) {
            type = other.type;
            llvmInstr = other.llvmInstr;
            if (other.Assign) {
                Assign = std::make_unique<AssignmentInfo>(other.Assign->formalArg, other.Assign->actualArg);
            } else {
                Assign.reset();
            }
        }
        return *this;
    }

    InstructionType getType() const { return type; }
    llvm::Instruction* getLLVMInstr() const { return llvmInstr; }
    std::unique_ptr<AssignmentInfo>& getAssign() { return Assign; }
    const std::unique_ptr<AssignmentInfo>& getAssign() const { return Assign; }

private:
    InstructionType type;
    llvm::Instruction* llvmInstr = nullptr;
    std::unique_ptr<AssignmentInfo> Assign;
};


class CFAModel {
private:
    std::unordered_map<uint32_t, std::unique_ptr<CFALocation>> locations;     // The locations of the CFA
    std::vector<std::unique_ptr<CFAEdge>> edges;                              // The edges of the CFA
    CFALocation* entryLocation;                                               // The entry location of the CFA
    CFALocation* exitLocation;                                                // The exit location of the CFA
    llvm::Function* func;                                                     // The function of the automaton
    void createCFAModel();
    
public:
    CFAModel(llvm::Function* func) : func(func) {
        createCFAModel();
    }
    ~CFAModel() {
        // We must clear edges before locations to avoid dangling pointers
        edges.clear();
        locations.clear();
    }

    CFALocation* getEntryLocation() const { return entryLocation; }
    void setEntryLocation(CFALocation* entryLocation) { this->entryLocation = entryLocation; }
    CFALocation* getExitLocation() const { return exitLocation; }
    void setExitLocation(CFALocation* exitLocation) { this->exitLocation = exitLocation; }
    llvm::Function* getFunc() const { return func; }
    CFALocation* getLocation(uint32_t locId) const { 
        auto it = locations.find(locId);
        return (it != locations.end()) ? it->second.get() : nullptr;
    }
    std::vector<CFALocation*> getLocations() const {
        std::vector<CFALocation*> locs;
        for (const auto& [locId, location] : locations) {
            locs.push_back(location.get());
        }
        return locs;
    }
    std::vector<CFAEdge*> getEdges() const {
        std::vector<CFAEdge*> edgeList;
        for (const auto& edge : edges) {
            edgeList.push_back(edge.get());
        }
        return edgeList;
    }
    bool hasLocation(const uint32_t locId) const { return locations.find(locId) != locations.end(); }

    CFALocation* createLocation(bool isEntry = false);
    CFAEdge* createEdge(CFALocation* srcLoc, CFALocation* dstLoc, EdgeType type);
    void removeLocation(uint32_t locId);
    void removeLocation(CFALocation* loc);
    void removeEdge(CFAEdge* edge);
    void removeEdges(CFALocation* src, CFALocation* dst);

    void ToDot(const std::string& filename) const;

};

class CFALocation {
private:
    uint32_t locId;                              // The ID of the location
    std::vector<CFAEdge*> incomingEdges;         // The incoming edges of the location
    std::vector<CFAEdge*> outgoingEdges;         // The outgoing edges of the location
    llvm::BasicBlock* bb;                         // The basic block of the location
public:
    CFALocation(uint32_t locId) : locId(locId) {}
    ~CFALocation() {
        incomingEdges.clear();
        outgoingEdges.clear();
    }

    uint32_t getLocId() const { return locId; }
    llvm::BasicBlock* getBB() const { return bb; }
    void setBB(llvm::BasicBlock* bb) { this->bb = bb; }
    void addIncomingEdge(CFAEdge* edge) { incomingEdges.push_back(edge); }
    void addOutgoingEdge(CFAEdge* edge) { outgoingEdges.push_back(edge); }
    void removeIncomingEdge(CFAEdge* edge) {
        incomingEdges.erase(std::remove(incomingEdges.begin(), incomingEdges.end(), edge), incomingEdges.end());
    }
    void removeOutgoingEdge(CFAEdge* edge) {
        outgoingEdges.erase(std::remove(outgoingEdges.begin(), outgoingEdges.end(), edge), outgoingEdges.end());
    }
    const std::vector<CFAEdge*>& getIncomingEdges() const { return incomingEdges; }
    const std::vector<CFAEdge*>& getOutgoingEdges() const { return outgoingEdges; }
    bool hasIncomingEdges() const { return !incomingEdges.empty(); }
    bool hasOutgoingEdges() const { return !outgoingEdges.empty(); }
};

enum class EdgeType {
    SEQUENTIAL,     // Sequential edge (fall-through)
    BRANCH_TRUE,    // Branch true edge
    BRANCH_FALSE,   // Branch false edge
    RETURN,         // Return edge
    CALL            // Call edge
};

class CFAEdge {
private:
    CFALocation* srcLoc;                            // The source location of the edge
    CFALocation* dstLoc;                            // The destination location of the edge
    std::vector<CFAInstruction> instructions;       // The instructions of the edge 
    llvm::Value* condBrVar = nullptr;               // The conditional branch variable of the edge
    EdgeType type;                                  // The type of the edge

public:
    CFAEdge(CFALocation* srcLoc, CFALocation* dstLoc, EdgeType type) : srcLoc(srcLoc), dstLoc(dstLoc), type(type) {
        // Add this edge to the source and destination locations
        if (!srcLoc || !dstLoc) {
            ERROR("CFAEdge: srcLoc or dstLoc is null");
            exit(1);
        }
        srcLoc->addOutgoingEdge(this);
        dstLoc->addIncomingEdge(this);
    }
    ~CFAEdge() {
        if (srcLoc) {
            srcLoc->removeOutgoingEdge(this);
        }
        if (dstLoc) {
            dstLoc->removeIncomingEdge(this);
        }
    }

    CFALocation* getSrcLoc() const { return srcLoc; }
    CFALocation* getDstLoc() const { return dstLoc; }
    EdgeType getType() const { return type; }
    const std::vector<CFAInstruction>& getInstructions() const { return instructions; }
    void addInstruction(CFAInstruction&& inst) { instructions.push_back(std::move(inst)); }
    void addLLVMInstruction(llvm::Instruction* inst) { instructions.emplace_back(inst); }
    void setCondBrVar(llvm::Value* condBrVar) { this->condBrVar = condBrVar; }
    bool hasCondBrVar() const { return condBrVar != nullptr; }
    llvm::Value* getCondBrVar() const { return condBrVar; }
    void addAssignmentInstruction(llvm::Value* formalArg, llvm::Value* actualArg) { 
        instructions.emplace_back(formalArg, actualArg); 
    }
    
    // Change the destination location of this edge
    void changeDstLoc(CFALocation* newDstLoc) {
        if (dstLoc) {
            dstLoc->removeIncomingEdge(this);
        }
        dstLoc = newDstLoc;
        if (newDstLoc) {
            newDstLoc->addIncomingEdge(this);
        }
    }
};


}
#endif
