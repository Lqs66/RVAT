#ifndef INSTRS_INTERPRETER_H
#define INSTRS_INTERPRETER_H

#include "Interpreter.h"
#include "model/CFAModel.h"

using namespace llvm;

namespace uppllvm {

struct CFAContext {
    std::map<Value *, GenericValue> Values; // LLVM values used in this invocation
    AllocaHolder Allocas;            // Track memory allocated by alloca
    CFAContext() : Values(), Allocas() {}
};

class InstrsInterpreter : public Interpreter {
    CFAContext cfaContext;  // Changed from CFAContext to cfaContext to avoid naming conflict

public:
    explicit InstrsInterpreter(std::unique_ptr<Module> M);
    ~InstrsInterpreter();


    /// Create an interpreter ExecutionEngine.
    ///
    static ExecutionEngine *create(std::unique_ptr<Module> M,
                                   std::string *ErrorStr = nullptr);
    
    CFAContext& getCFAContext() { return cfaContext; }
    
    /// Set entry point function parameters using SetValue
    void setEntryPointParams(Function* entryFunc, ArrayRef<GenericValue> ArgValues);
                            
    /// Run the instructions.
    void runInstrs(std::vector<CFAInstruction>& Instrs) override;
    
    void visitCFAInstruction(CFAInstruction &I);

    void CFAVisitReturnInst(ReturnInst &I);
    void CFAVisitBranchInst(BranchInst &I) {
        llvm_unreachable("BranchInst not supported in InstrsInterpreter");
    }
    void CFAVisitSwitchInst(SwitchInst &I) {
        llvm_unreachable("SwitchInst not supported in InstrsInterpreter");
    }
    void CFAVisitIndirectBrInst(IndirectBrInst &I) {
        llvm_unreachable("IndirectBrInst not supported in InstrsInterpreter");
    }

    void CFAVisitPHINode(PHINode &PN) {
        llvm_unreachable("PHI nodes already handled!");
    }
    void CFAVisitUnaryOperator(UnaryOperator &I);
    void CFAVisitBinaryOperator(BinaryOperator &I);
    void CFAVisitICmpInst(ICmpInst &I);
    void CFAVisitFCmpInst(FCmpInst &I);
    void CFAVisitAllocaInst(AllocaInst &I);
    void CFAVisitLoadInst(LoadInst &I);
    void CFAVisitStoreInst(StoreInst &I);
    void CFAVisitGetElementPtrInst(GetElementPtrInst &I);
    void CFAVisitTruncInst(TruncInst &I);
    void CFAVisitZExtInst(ZExtInst &I);
    void CFAVisitSExtInst(SExtInst &I);
    void CFAVisitFPTruncInst(FPTruncInst &I);
    void CFAVisitFPExtInst(FPExtInst &I);
    void CFAVisitUIToFPInst(UIToFPInst &I);
    void CFAVisitSIToFPInst(SIToFPInst &I);
    void CFAVisitFPToUIInst(FPToUIInst &I);
    void CFAVisitFPToSIInst(FPToSIInst &I);
    void CFAVisitPtrToIntInst(PtrToIntInst &I);
    void CFAVisitIntToPtrInst(IntToPtrInst &I);
    void CFAVisitBitCastInst(BitCastInst &I);
    void CFAVisitSelectInst(SelectInst &I);

    void CFAVisitCallBase(CallBase &I);
    
    void CFAVisitVAEndInst(VAEndInst &I) {
        // noop
    }
    void CFAVisitUnreachableInst(UnreachableInst &I) {
        report_fatal_error("Program executed an 'unreachable' instruction!");
    }
    void CFAVisitVAStartInst(VAStartInst &I);
    void CFAVisitVACopyInst(VACopyInst &I);
    void CFAVisitIntrinsicInst(IntrinsicInst &I);

    void CFAVisitShl(BinaryOperator &I);
    void CFAVisitLShr(BinaryOperator &I);
    void CFAVisitAShr(BinaryOperator &I);

    void CFAVisitVAArgInst(VAArgInst &I);
    void CFAVisitExtractElementInst(ExtractElementInst &I);
    void CFAVisitInsertElementInst(InsertElementInst &I);
    void CFAVisitShuffleVectorInst(ShuffleVectorInst &I);

    void CFAVisitExtractValueInst(ExtractValueInst &I);
    void CFAVisitInsertValueInst(InsertValueInst &I);

    void CFAVisitInstruction(Instruction &I) {
        errs() << I << "\n";
        llvm_unreachable("Instruction not interpretable yet!");
    }
private:
    void SetValue(Value *V, GenericValue Val);

    GenericValue executeCFAAssignment(CFAInstruction &I);
    
    // modified from Execution.cpp
    GenericValue executeGEPOperation(Value *Ptr, llvm::gep_type_iterator I,
                                                 llvm::gep_type_iterator E);
    GenericValue getConstantExprValue(ConstantExpr *CE);
    GenericValue getOperandValue(Value *V);
    GenericValue executeTruncInst(Value *SrcVal, Type *DstTy);
    GenericValue executeSExtInst(Value *SrcVal, Type *DstTy);
    GenericValue executeZExtInst(Value *SrcVal, Type *DstTy);
    GenericValue executeFPTruncInst(Value *SrcVal, Type *DstTy);
    GenericValue executeFPExtInst(Value *SrcVal, Type *DstTy);
    GenericValue executeFPToUIInst(Value *SrcVal, Type *DstTy);
    GenericValue executeFPToSIInst(Value *SrcVal, Type *DstTy);
    GenericValue executeUIToFPInst(Value *SrcVal, Type *DstTy);
    GenericValue executeSIToFPInst(Value *SrcVal, Type *DstTy);
    GenericValue executePtrToIntInst(Value *SrcVal, Type *DstTy);
    GenericValue executeIntToPtrInst(Value *SrcVal, Type *DstTy);
    GenericValue executeBitCastInst(Value *SrcVal, Type *DstTy);
    GenericValue executeCastOperation(Instruction::CastOps opcode, Value *SrcVal, Type *Ty); // Why Execution.cpp don't have these implementations?
    
    // Converted from static helpers in Execution.cpp to member functions
    // (CFA-context-based execution). Grouped here for clarity.
    void executeFNegInst(GenericValue &Dest, GenericValue Src, Type *Ty);
    void executeFAddInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
    void executeFSubInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
    void executeFMulInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
    void executeFDivInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
    void executeFRemInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_EQ(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_NE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_ULT(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_SLT(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_UGT(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_SGT(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_ULE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_SLE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_UGE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeICMP_SGE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_OEQ(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_ONE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_OLE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_OGE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_OLT(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_OGT(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_UEQ(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_UNE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_ULE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_UGE(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_ULT(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_UGT(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_ORD(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_UNO(GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeFCMP_BOOL(GenericValue Src1, GenericValue Src2, Type *Ty, bool val);
    GenericValue executeCmpInst(unsigned predicate, GenericValue Src1, GenericValue Src2, Type *Ty);
    GenericValue executeSelectInst(GenericValue Src1, GenericValue Src2, GenericValue Src3, Type *Ty);
};

}
#endif