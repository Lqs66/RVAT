#include "InstrsInterpreter.h"
#include "model/CFAModel.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cmath>
using namespace llvm;
using namespace uppllvm;

#define DEBUG_TYPE "cfa-interpreter"

static cl::opt<bool> PrintVolatile("cfa-interpreter-print-volatile", cl::Hidden,
          cl::desc("make the CFA interpreter print every volatile load and store"));

static unsigned getShiftAmount(uint64_t orgShiftAmount,
                               llvm::APInt valueToShift) {
  unsigned valueWidth = valueToShift.getBitWidth();
  if (orgShiftAmount < (uint64_t)valueWidth)
    return orgShiftAmount;
  // according to the llvm documentation, if orgShiftAmount > valueWidth,
  // the result is undefined. but we do shift by this rule:
  return (NextPowerOf2(valueWidth-1) - 1) & orgShiftAmount;
}

void InstrsInterpreter::SetValue(Value *V, GenericValue Val) {
    cfaContext.Values[V] = Val;
}

void InstrsInterpreter::setEntryPointParams(Function* entryFunc, ArrayRef<GenericValue> ArgValues) {
    if (!entryFunc) {
        ERROR("Entry point function is null. Cannot set parameters.");
        abort();
    }
    
    // Validate argument count
    unsigned expectedArgs = entryFunc->arg_size();
    unsigned providedArgs = ArgValues.size();
    
    if (providedArgs != expectedArgs) {
        ERROR("Not enough arguments provided. Expected " + std::to_string(expectedArgs) + ", got " + std::to_string(providedArgs) + ".");
        abort();
    }
    
    // Set values for each function argument
    unsigned i = 0;
    for (auto &Arg : entryFunc->args()) {
        if (i < providedArgs) {
            SetValue(&Arg, ArgValues[i]);
        }
        i++;
    }
    
    // Note: If there are more arguments than parameters (varargs case),
    // they are ignored here. Varargs handling would require additional logic.
}

//===----------------------------------------------------------------------===//
//                    Instruction Visit Methods
//===----------------------------------------------------------------------===//

void InstrsInterpreter::CFAVisitReturnInst(ReturnInst &I) {
  Type *RetTy = Type::getVoidTy(I.getContext());
  GenericValue Result;

  // Save away the return value... (if we are not 'ret void')
  if (I.getNumOperands()) {
    RetTy  = I.getReturnValue()->getType();
    Result = getOperandValue(I.getReturnValue());
    llvm::outs() << "CFA Return value: " << Result.IntVal << "\n";
  }
}

void InstrsInterpreter::CFAVisitUnaryOperator(UnaryOperator &I) {
    Type *Ty = I.getOperand(0)->getType();
    GenericValue Src = getOperandValue(I.getOperand(0));
    GenericValue R; // Result

    // First process vector operation
    if (Ty->isVectorTy()) {
      R.AggregateVal.resize(Src.AggregateVal.size());

      switch(I.getOpcode()) {
      default:
        llvm_unreachable("Don't know how to handle this unary operator");
        break;
      case Instruction::FNeg:
        if (cast<VectorType>(Ty)->getElementType()->isFloatTy()) {
          for (unsigned i = 0; i < R.AggregateVal.size(); ++i)
            R.AggregateVal[i].FloatVal = -Src.AggregateVal[i].FloatVal;
        } else if (cast<VectorType>(Ty)->getElementType()->isDoubleTy()) {
          for (unsigned i = 0; i < R.AggregateVal.size(); ++i)
            R.AggregateVal[i].DoubleVal = -Src.AggregateVal[i].DoubleVal;
        } else {
          llvm_unreachable("Unhandled type for FNeg instruction");
        }
        break;
      }
    } else {
      switch (I.getOpcode()) {
      default:
        llvm_unreachable("Don't know how to handle this unary operator");
        break;
      case Instruction::FNeg: executeFNegInst(R, Src, Ty); break;
      }
    }
    SetValue(&I, R);
}

void InstrsInterpreter::CFAVisitCallBase(CallBase &I) {
  // CFA-specific version that uses getOperandValue without ExecutionContext
  std::vector<GenericValue> ArgVals;
  const unsigned NumArgs = I.arg_size();
  ArgVals.reserve(NumArgs);
  for (Value *V : I.args())
    ArgVals.push_back(getOperandValue(V));

  // To handle indirect calls, we must get the pointer value from the argument
  // and treat it as a function pointer.
  GenericValue SRC = getOperandValue(I.getCalledOperand());
  Function *F = (Function*)GVTOP(SRC);
  // if (F->getName().str() == "clock_gettime") {
  //   GenericValue Result;
  //   // Create APInt with proper bit width (64 bits for typical return value)
  //   Result.IntVal = APInt(64, 10000000000ULL); // 10 seconds in nanoseconds
  //   llvm::outs() << "CFA Simulated clock_gettime returning " << Result.IntVal << " ns\n";
  //   SetValue(&I, Result);
  //   return;
  // }

  // Prepare the calling stack frame for callFunction
  // callFunction needs a valid calling context to handle return values
  if (getEStack().empty()) {
    getEStack().emplace_back();
  }
  size_t stackSizeBeforeCall = getEStack().size();
  ExecutionContext &CallingSF = getEStack().back();
  CallingSF.Caller = &I;
  
  // Call callFunction first - it handles external functions directly
  // For external functions, callFunction calls callExternalFunction and returns immediately
  // For internal functions, callFunction sets up the stack frame
  callFunction(F, ArgVals);
  
  GenericValue Result;
  
  // Check if the function was external (callFunction already handled it)
  if (F->isDeclaration()) {
    // External function: callFunction already called callExternalFunction
    // and set the return value in the calling frame's Values
    if (!I.getType()->isVoidTy() && CallingSF.Values.count(&I)) {
      Result = CallingSF.Values[&I];
    }
  } else {
    // Internal function: callFunction set up the stack frame, now execute it
    // Execute the function completely using run()
    // This ensures all instructions are executed using Interpreter's methods
    while (!getEStack().empty() && getEStack().size() > stackSizeBeforeCall) {
      ExecutionContext &SF = getEStack().back();
      if (SF.CurInst == SF.CurBB->end()) {
        // Reached end of basic block without terminator - shouldn't happen
        break;
      }
      Instruction &Instr = *SF.CurInst++;
      LLVM_DEBUG(dbgs() << "About to interpret: " << Instr << "\n");
      visit(Instr);  // Dispatch to Interpreter's visit* methods
    }
    
    // After execution, the called function's frame has been popped by popStackAndReturnValueToCaller
    // We need to re-get the calling frame reference because the stack may have changed
    if (!getEStack().empty() && getEStack().size() == stackSizeBeforeCall) {
      ExecutionContext &CallingSFAfter = getEStack().back();
      // popStackAndReturnValueToCaller sets the return value in the calling frame's Values[Caller]
      // Get the return value from the calling frame
      if (!I.getType()->isVoidTy() && CallingSFAfter.Values.count(&I)) {
        Result = CallingSFAfter.Values[&I];
        // llvm::outs() << "Return value: " << Result.IntVal << "\n";
      } else if (!I.getType()->isVoidTy()) {
        llvm::outs() << "Warning: Return value not found in CallingSF.Values for " << I << "\n";
        llvm::outs() << "Stack size: " << getEStack().size() << ", expected: " << stackSizeBeforeCall << "\n";
        llvm::outs() << "Caller pointer: " << (CallingSFAfter.Caller == &I ? "matches" : "mismatch") << "\n";
      }
    } else {
      llvm::outs() << "Error: Stack size mismatch after function execution. Expected: " 
                   << stackSizeBeforeCall << ", actual: " << getEStack().size() << "\n";
    }
  }
  
  // Clear the Caller pointer (use current stack state)
  if (!getEStack().empty()) {
    ExecutionContext &CurrentSF = getEStack().back();
    if (CurrentSF.Caller == &I) {
      CurrentSF.Caller = nullptr;
    }
  }
  
  // Store the return value in CFA context for the call instruction
  // This allows subsequent CFA instructions to use the return value
  if (!I.getType()->isVoidTy()) {
    // llvm::errs() << "Return value: " << Result.PointerVal << "\n";
    SetValue(&I, Result);
  }

  // llvm::outs() << "Return value: " << cfaContext.Values[&I].IntVal << "\n";
}

void InstrsInterpreter::CFAVisitBinaryOperator(BinaryOperator &I) {
  Type *Ty    = I.getOperand(0)->getType();
  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue R;   // Result

  // First process vector operation
  if (Ty->isVectorTy()) {
    assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());
    R.AggregateVal.resize(Src1.AggregateVal.size());

    // Macros to execute binary operation 'OP' over integer vectors
#define INTEGER_VECTOR_OPERATION(OP)                               \
    for (unsigned i = 0; i < R.AggregateVal.size(); ++i)           \
      R.AggregateVal[i].IntVal =                                   \
      Src1.AggregateVal[i].IntVal OP Src2.AggregateVal[i].IntVal;

    // Additional macros to execute binary operations udiv/sdiv/urem/srem since
    // they have different notation.
#define INTEGER_VECTOR_FUNCTION(OP)                                \
    for (unsigned i = 0; i < R.AggregateVal.size(); ++i)           \
      R.AggregateVal[i].IntVal =                                   \
      Src1.AggregateVal[i].IntVal.OP(Src2.AggregateVal[i].IntVal);

    // Macros to execute binary operation 'OP' over floating point type TY
    // (float or double) vectors
#define FLOAT_VECTOR_FUNCTION(OP, TY)                               \
      for (unsigned i = 0; i < R.AggregateVal.size(); ++i)          \
        R.AggregateVal[i].TY =                                      \
        Src1.AggregateVal[i].TY OP Src2.AggregateVal[i].TY;

    // Macros to choose appropriate TY: float or double and run operation
    // execution
#define FLOAT_VECTOR_OP(OP) {                                         \
  if (cast<VectorType>(Ty)->getElementType()->isFloatTy())            \
    FLOAT_VECTOR_FUNCTION(OP, FloatVal)                               \
  else {                                                              \
    if (cast<VectorType>(Ty)->getElementType()->isDoubleTy())         \
      FLOAT_VECTOR_FUNCTION(OP, DoubleVal)                            \
    else {                                                            \
      dbgs() << "Unhandled type for OP instruction: " << *Ty << "\n"; \
      llvm_unreachable(0);                                            \
    }                                                                 \
  }                                                                   \
}

    switch(I.getOpcode()){
    default:
      dbgs() << "Don't know how to handle this binary operator!\n-->" << I;
      llvm_unreachable(nullptr);
      break;
    case Instruction::Add:   INTEGER_VECTOR_OPERATION(+) break;
    case Instruction::Sub:   INTEGER_VECTOR_OPERATION(-) break;
    case Instruction::Mul:   INTEGER_VECTOR_OPERATION(*) break;
    case Instruction::UDiv:  INTEGER_VECTOR_FUNCTION(udiv) break;
    case Instruction::SDiv:  INTEGER_VECTOR_FUNCTION(sdiv) break;
    case Instruction::URem:  INTEGER_VECTOR_FUNCTION(urem) break;
    case Instruction::SRem:  INTEGER_VECTOR_FUNCTION(srem) break;
    case Instruction::And:   INTEGER_VECTOR_OPERATION(&) break;
    case Instruction::Or:    INTEGER_VECTOR_OPERATION(|) break;
    case Instruction::Xor:   INTEGER_VECTOR_OPERATION(^) break;
    case Instruction::FAdd:  FLOAT_VECTOR_OP(+) break;
    case Instruction::FSub:  FLOAT_VECTOR_OP(-) break;
    case Instruction::FMul:  FLOAT_VECTOR_OP(*) break;
    case Instruction::FDiv:  FLOAT_VECTOR_OP(/) break;
    case Instruction::FRem:
      if (cast<VectorType>(Ty)->getElementType()->isFloatTy())
        for (unsigned i = 0; i < R.AggregateVal.size(); ++i)
          R.AggregateVal[i].FloatVal =
          fmod(Src1.AggregateVal[i].FloatVal, Src2.AggregateVal[i].FloatVal);
      else {
        if (cast<VectorType>(Ty)->getElementType()->isDoubleTy())
          for (unsigned i = 0; i < R.AggregateVal.size(); ++i)
            R.AggregateVal[i].DoubleVal =
            fmod(Src1.AggregateVal[i].DoubleVal, Src2.AggregateVal[i].DoubleVal);
        else {
          dbgs() << "Unhandled type for Rem instruction: " << *Ty << "\n";
          llvm_unreachable(nullptr);
        }
      }
      break;
    }
  } else {
    switch (I.getOpcode()) {
    default:
      dbgs() << "Don't know how to handle this binary operator!\n-->" << I;
      llvm_unreachable(nullptr);
      break;
    case Instruction::Add:   R.IntVal = Src1.IntVal + Src2.IntVal; break;
    case Instruction::Sub:   R.IntVal = Src1.IntVal - Src2.IntVal; break;
    case Instruction::Mul:   R.IntVal = Src1.IntVal * Src2.IntVal; break;
    case Instruction::FAdd:  executeFAddInst(R, Src1, Src2, Ty); break;
    case Instruction::FSub:  executeFSubInst(R, Src1, Src2, Ty); break;
    case Instruction::FMul:  executeFMulInst(R, Src1, Src2, Ty); break;
    case Instruction::FDiv:  executeFDivInst(R, Src1, Src2, Ty); break;
    case Instruction::FRem:  executeFRemInst(R, Src1, Src2, Ty); break;
    case Instruction::UDiv:  R.IntVal = Src1.IntVal.udiv(Src2.IntVal); break;
    case Instruction::SDiv:  R.IntVal = Src1.IntVal.sdiv(Src2.IntVal); break;
    case Instruction::URem:  R.IntVal = Src1.IntVal.urem(Src2.IntVal); break;
    case Instruction::SRem:  R.IntVal = Src1.IntVal.srem(Src2.IntVal); break;
    case Instruction::And:   R.IntVal = Src1.IntVal & Src2.IntVal; break;
    case Instruction::Or:    R.IntVal = Src1.IntVal | Src2.IntVal; break;
    case Instruction::Xor:   R.IntVal = Src1.IntVal ^ Src2.IntVal; break;
    }
  }
  SetValue(&I, R);
}

void InstrsInterpreter::CFAVisitICmpInst(ICmpInst &I) {
  Type *Ty    = I.getOperand(0)->getType();
  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue R;   // Result

  switch (I.getPredicate()) {
  case ICmpInst::ICMP_EQ:  R = executeICMP_EQ(Src1,  Src2, Ty); break;
  case ICmpInst::ICMP_NE:  R = executeICMP_NE(Src1,  Src2, Ty); break;
  case ICmpInst::ICMP_ULT: R = executeICMP_ULT(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_SLT: R = executeICMP_SLT(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_UGT: R = executeICMP_UGT(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_SGT: R = executeICMP_SGT(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_ULE: R = executeICMP_ULE(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_SLE: R = executeICMP_SLE(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_UGE: R = executeICMP_UGE(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_SGE: R = executeICMP_SGE(Src1, Src2, Ty); break;
  default:
    dbgs() << "Don't know how to handle this ICmp predicate!\n-->" << I;
    llvm_unreachable(nullptr);
  }

  SetValue(&I, R);
}

void InstrsInterpreter::CFAVisitFCmpInst(FCmpInst &I) {
  Type *Ty    = I.getOperand(0)->getType();
  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue R;   // Result

  switch (I.getPredicate()) {
  default:
    dbgs() << "Don't know how to handle this FCmp predicate!\n-->" << I;
    llvm_unreachable(nullptr);
  break;
  case FCmpInst::FCMP_FALSE: R = executeFCMP_BOOL(Src1, Src2, Ty, false);
  break;
  case FCmpInst::FCMP_TRUE:  R = executeFCMP_BOOL(Src1, Src2, Ty, true);
  break;
  case FCmpInst::FCMP_ORD:   R = executeFCMP_ORD(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UNO:   R = executeFCMP_UNO(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UEQ:   R = executeFCMP_UEQ(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OEQ:   R = executeFCMP_OEQ(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UNE:   R = executeFCMP_UNE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_ONE:   R = executeFCMP_ONE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_ULT:   R = executeFCMP_ULT(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OLT:   R = executeFCMP_OLT(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UGT:   R = executeFCMP_UGT(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OGT:   R = executeFCMP_OGT(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_ULE:   R = executeFCMP_ULE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OLE:   R = executeFCMP_OLE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UGE:   R = executeFCMP_UGE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OGE:   R = executeFCMP_OGE(Src1, Src2, Ty); break;
  }

  SetValue(&I, R);
}

void InstrsInterpreter::CFAVisitAllocaInst(AllocaInst &I) {
  Type *Ty = I.getAllocatedType(); // Type to be allocated

  // Get the number of elements being allocated by the array...
  unsigned NumElements =
    getOperandValue(I.getOperand(0)).IntVal.getZExtValue();

  unsigned TypeSize = (size_t)getDataLayout().getTypeAllocSize(Ty);

  // Avoid malloc-ing zero bytes, use max()...
  unsigned MemToAlloc = std::max(1U, NumElements * TypeSize);

  // Allocate enough memory to hold the type...
  void *Memory = safe_malloc(MemToAlloc);
  
  // Initialize allocated memory to zero to avoid reading garbage values
  memset(Memory, 0, MemToAlloc);

  LLVM_DEBUG(dbgs() << "Allocated Type: " << *Ty << " (" << TypeSize
                    << " bytes) x " << NumElements << " (Total: " << MemToAlloc
                    << ") at " << uintptr_t(Memory) << '\n');

  GenericValue Result = PTOGV(Memory);
  assert(Result.PointerVal && "Null pointer returned by malloc!");
  SetValue(&I, Result);
  // llvm::outs() << "AllocaInst: " << I << " with value: " << Result.PointerVal << "\n";
  if (I.getOpcode() == Instruction::Alloca)
    cfaContext.Allocas.add(Memory);
}

void InstrsInterpreter::CFAVisitLoadInst(LoadInst &I) {
  GenericValue SRC = getOperandValue(I.getPointerOperand());
  GenericValue *Ptr = (GenericValue*)GVTOP(SRC);
  GenericValue Result;
  // llvm::outs() << "LoadInst: " << I << "\n";
  // llvm::outs() << "BB: " << I.getParent()->getName() << "\n";
  // llvm::outs() << "Func: " << I.getParent()->getParent()->getName() << "\n";
  // llvm::outs() << "Load SRC: " << SRC.PointerVal 
  //              << " (decimal: " << (uintptr_t)SRC.PointerVal << ")\n";
  // llvm::outs() << "Load Ptr: " << Ptr 
  //              << " (decimal: " << (uintptr_t)Ptr << ")\n";
  // llvm::outs() << "=========================================" << "\n";
  LoadValueFromMemory(Result, Ptr, I.getType());
  SetValue(&I, Result);
  if (I.isVolatile() && PrintVolatile)
    dbgs() << "Volatile load " << I;
}

void InstrsInterpreter::CFAVisitStoreInst(StoreInst &I) {
  GenericValue Val = getOperandValue(I.getOperand(0));
  GenericValue SRC = getOperandValue(I.getPointerOperand());
  // llvm::errs() << "StoreInst: " << I << "\n";
  // llvm::errs() << "BB: " << I.getParent()->getName() << "\n";
  // llvm::errs() << "Func: " << I.getParent()->getParent()->getName() << "\n";
  // llvm::errs() << "Store Val: " << Val.PointerVal << "\n";
  // llvm::errs() << "Store SRC: " << SRC.PointerVal << "\n";
  // llvm::errs() << "=========================================" << "\n";
  StoreValueToMemory(Val, (GenericValue *)GVTOP(SRC),
                     I.getOperand(0)->getType());
  if (I.isVolatile() && PrintVolatile)
    dbgs() << "Volatile store: " << I;
}

void InstrsInterpreter::CFAVisitGetElementPtrInst(GetElementPtrInst &I) {
  SetValue(&I, executeGEPOperation(I.getPointerOperand(),
                                   gep_type_begin(I), gep_type_end(I)));
}

void InstrsInterpreter::CFAVisitTruncInst(TruncInst &I) {
  SetValue(&I, executeTruncInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitSExtInst(SExtInst &I) {
  SetValue(&I, executeSExtInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitFPTruncInst(FPTruncInst &I) {
  SetValue(&I, executeFPTruncInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitFPExtInst(FPExtInst &I) {
  SetValue(&I, executeFPExtInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitUIToFPInst(UIToFPInst &I) {
  SetValue(&I, executeUIToFPInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitSIToFPInst(SIToFPInst &I) {
  SetValue(&I, executeSIToFPInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitFPToUIInst(FPToUIInst &I) {
  SetValue(&I, executeFPToUIInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitFPToSIInst(FPToSIInst &I) {
  SetValue(&I, executeFPToSIInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitZExtInst(ZExtInst &I) {
  SetValue(&I, executeZExtInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitPtrToIntInst(PtrToIntInst &I) {
  SetValue(&I, executePtrToIntInst(I.getOperand(0), I.getType()));   
}

void InstrsInterpreter::CFAVisitIntToPtrInst(IntToPtrInst &I) {
  SetValue(&I, executeIntToPtrInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitBitCastInst(BitCastInst &I) {
  SetValue(&I, executeBitCastInst(I.getOperand(0), I.getType()));
}

void InstrsInterpreter::CFAVisitSelectInst(SelectInst &I) {
  Type * Ty = I.getOperand(0)->getType();
  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue Src3 = getOperandValue(I.getOperand(2));
  GenericValue R = executeSelectInst(Src1, Src2, Src3, Ty);
  SetValue(&I, R);
}

void InstrsInterpreter::CFAVisitVAStartInst(VAStartInst &I) {
  GenericValue ArgIndex;
  // In CFA execution mode, we simulate a single stack frame
  ArgIndex.UIntPairVal.first = 0;
  ArgIndex.UIntPairVal.second = 0;
  SetValue(&I, ArgIndex);  
}

void InstrsInterpreter::CFAVisitVACopyInst(VACopyInst &I) {
  SetValue(&I, getOperandValue(*I.arg_begin()));
}

void InstrsInterpreter::CFAVisitIntrinsicInst(IntrinsicInst &I) {

  switch (I.getIntrinsicID()) {
    case Intrinsic::dbg_declare:
    case Intrinsic::dbg_value:
    case Intrinsic::dbg_addr:
    case Intrinsic::dbg_label:
      // Debug intrinsics are no-ops in the interpreter
      return;
    case Intrinsic::fmuladd:
    case Intrinsic::fma: {
      // fmuladd(a, b, c) = a * b + c
      GenericValue Op0 = getOperandValue(I.getOperand(0));
      GenericValue Op1 = getOperandValue(I.getOperand(1));
      GenericValue Op2 = getOperandValue(I.getOperand(2));
      GenericValue Result;
      
      Type *Ty = I.getType();
      if (Ty->isFloatTy()) {
        Result.FloatVal = Op0.FloatVal * Op1.FloatVal + Op2.FloatVal;
      } else if (Ty->isDoubleTy()) {
        Result.DoubleVal = Op0.DoubleVal * Op1.DoubleVal + Op2.DoubleVal;
      } else {
        llvm_unreachable("Unsupported fmuladd type");
      }
      
      SetValue(&I, Result);
      return;
    }
    case Intrinsic::uadd_with_overflow:
    case Intrinsic::sadd_with_overflow:
    case Intrinsic::usub_with_overflow:
    case Intrinsic::ssub_with_overflow:
    case Intrinsic::umul_with_overflow:
    case Intrinsic::smul_with_overflow: {
      // These intrinsics return a struct {result, overflow_flag}
      GenericValue Op0 = getOperandValue(I.getOperand(0));
      GenericValue Op1 = getOperandValue(I.getOperand(1));
      GenericValue Result;
      
      // Result is a struct with 2 elements: {result_value, overflow_bool}
      Result.AggregateVal.resize(2);
      
      APInt &A = Op0.IntVal;
      APInt &B = Op1.IntVal;
      bool overflow = false;
      
      switch (I.getIntrinsicID()) {
        case Intrinsic::uadd_with_overflow:
          Result.AggregateVal[0].IntVal = A + B;
          overflow = Result.AggregateVal[0].IntVal.ult(A);
          break;
        case Intrinsic::sadd_with_overflow:
          Result.AggregateVal[0].IntVal = A + B;
          overflow = A.isNonNegative() == B.isNonNegative() &&
                     Result.AggregateVal[0].IntVal.isNonNegative() != A.isNonNegative();
          break;
        case Intrinsic::usub_with_overflow:
          Result.AggregateVal[0].IntVal = A - B;
          overflow = A.ult(B);
          break;
        case Intrinsic::ssub_with_overflow:
          Result.AggregateVal[0].IntVal = A - B;
          overflow = A.isNonNegative() != B.isNonNegative() &&
                     Result.AggregateVal[0].IntVal.isNonNegative() != A.isNonNegative();
          break;
        case Intrinsic::umul_with_overflow: {
          Result.AggregateVal[0].IntVal = A * B;
          // Check for unsigned overflow: if A != 0 and (A * B) / A != B
          if (!A.isZero() && Result.AggregateVal[0].IntVal.udiv(A) != B) {
            overflow = true;
          }
          break;
        }
        case Intrinsic::smul_with_overflow: {
          Result.AggregateVal[0].IntVal = A * B;
          // Check for signed overflow
          if (!A.isZero() && !B.isZero()) {
            APInt Div = Result.AggregateVal[0].IntVal.sdiv(A);
            if (Div != B) {
              overflow = true;
            }
          }
          // Also check for edge case: most negative number * -1
          if (A.isMinSignedValue() && B.isAllOnes()) {
            overflow = true;
          }
          if (B.isMinSignedValue() && A.isAllOnes()) {
            overflow = true;
          }
          break;
        }
        default:
          llvm_unreachable("Unknown overflow intrinsic");
      }
      
      // Set overflow flag (second element of the struct)
      Result.AggregateVal[1].IntVal = APInt(1, overflow ? 1 : 0);
      
      SetValue(&I, Result);
      return;
    }
    case Intrinsic::fabs: {
      // fabs(x) - absolute value for floating point
      GenericValue Op0 = getOperandValue(I.getOperand(0));
      GenericValue Result;
      
      Type *Ty = I.getType();
      if (Ty->isFloatTy()) {
        Result.FloatVal = std::fabs(Op0.FloatVal);
      } else if (Ty->isDoubleTy()) {
        Result.DoubleVal = std::fabs(Op0.DoubleVal);
      } else if (Ty->isVectorTy()) {
        // Handle vector fabs
        unsigned numElems = Op0.AggregateVal.size();
        Result.AggregateVal.resize(numElems);
        Type *ElemTy = cast<VectorType>(Ty)->getElementType();
        if (ElemTy->isFloatTy()) {
          for (unsigned i = 0; i < numElems; ++i) {
            Result.AggregateVal[i].FloatVal = std::fabs(Op0.AggregateVal[i].FloatVal);
          }
        } else if (ElemTy->isDoubleTy()) {
          for (unsigned i = 0; i < numElems; ++i) {
            Result.AggregateVal[i].DoubleVal = std::fabs(Op0.AggregateVal[i].DoubleVal);
          }
        } else {
          llvm_unreachable("Unsupported fabs vector element type");
        }
      } else {
        llvm_unreachable("Unsupported fabs type");
      }
      
      SetValue(&I, Result);
      return;
    }
    case Intrinsic::rint:
    case Intrinsic::nearbyint: {
      // Single-argument math functions (not handled by IntrinsicLowering)
      GenericValue Op0 = getOperandValue(I.getOperand(0));
      GenericValue Result;
      
      Type *Ty = I.getType();
      auto applyMathFunc = [&](double val) -> double {
        switch (I.getIntrinsicID()) {
          case Intrinsic::rint: return std::rint(val);
          case Intrinsic::nearbyint: return std::nearbyint(val);
          default: return val;
        }
      };
      
      if (Ty->isFloatTy()) {
        Result.FloatVal = static_cast<float>(applyMathFunc(Op0.FloatVal));
      } else if (Ty->isDoubleTy()) {
        Result.DoubleVal = applyMathFunc(Op0.DoubleVal);
      } else if (Ty->isVectorTy()) {
        unsigned numElems = Op0.AggregateVal.size();
        Result.AggregateVal.resize(numElems);
        Type *ElemTy = cast<VectorType>(Ty)->getElementType();
        if (ElemTy->isFloatTy()) {
          for (unsigned i = 0; i < numElems; ++i) {
            Result.AggregateVal[i].FloatVal = static_cast<float>(
              applyMathFunc(Op0.AggregateVal[i].FloatVal));
          }
        } else if (ElemTy->isDoubleTy()) {
          for (unsigned i = 0; i < numElems; ++i) {
            Result.AggregateVal[i].DoubleVal = applyMathFunc(Op0.AggregateVal[i].DoubleVal);
          }
        } else {
          llvm_unreachable("Unsupported math intrinsic vector element type");
        }
      } else {
        llvm_unreachable("Unsupported math intrinsic type");
      }
      
      SetValue(&I, Result);
      return;
    }
    case Intrinsic::minnum:
    case Intrinsic::maxnum: {
      // Two-argument math functions (not handled by IntrinsicLowering)
      GenericValue Op0 = getOperandValue(I.getOperand(0));
      GenericValue Op1 = getOperandValue(I.getOperand(1));
      GenericValue Result;
      
      Type *Ty = I.getType();
      auto applyMathFunc2 = [&](double val1, double val2) -> double {
        switch (I.getIntrinsicID()) {
          case Intrinsic::minnum: return std::fmin(val1, val2);
          case Intrinsic::maxnum: return std::fmax(val1, val2);
          default: return val1;
        }
      };
      
      if (Ty->isFloatTy()) {
        Result.FloatVal = static_cast<float>(applyMathFunc2(Op0.FloatVal, Op1.FloatVal));
      } else if (Ty->isDoubleTy()) {
        Result.DoubleVal = applyMathFunc2(Op0.DoubleVal, Op1.DoubleVal);
      } else if (Ty->isVectorTy()) {
        unsigned numElems = Op0.AggregateVal.size();
        Result.AggregateVal.resize(numElems);
        Type *ElemTy = cast<VectorType>(Ty)->getElementType();
        if (ElemTy->isFloatTy()) {
          for (unsigned i = 0; i < numElems; ++i) {
            Result.AggregateVal[i].FloatVal = static_cast<float>(
              applyMathFunc2(Op0.AggregateVal[i].FloatVal, Op1.AggregateVal[i].FloatVal));
          }
        } else if (ElemTy->isDoubleTy()) {
          for (unsigned i = 0; i < numElems; ++i) {
            Result.AggregateVal[i].DoubleVal = 
              applyMathFunc2(Op0.AggregateVal[i].DoubleVal, Op1.AggregateVal[i].DoubleVal);
          }
        } else {
          llvm_unreachable("Unsupported math intrinsic vector element type");
        }
      } else {
        llvm_unreachable("Unsupported math intrinsic type");
      }
      
      SetValue(&I, Result);
      return;
    }
    default:
      dbgs() << "Unhandled intrinsic: " << I << "\n";
      llvm_unreachable(nullptr);
  }
}

void InstrsInterpreter::CFAVisitShl(BinaryOperator &I) {
  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue Dest;
  Type *Ty = I.getType();

  if (Ty->isVectorTy()) {
    uint32_t src1Size = uint32_t(Src1.AggregateVal.size());
    assert(src1Size == Src2.AggregateVal.size());
    for (unsigned i = 0; i < src1Size; i++) {
      GenericValue Result;
      uint64_t shiftAmount = Src2.AggregateVal[i].IntVal.getZExtValue();
      llvm::APInt valueToShift = Src1.AggregateVal[i].IntVal;
      Result.IntVal = valueToShift.shl(getShiftAmount(shiftAmount, valueToShift));
      Dest.AggregateVal.push_back(Result);
    }
  } else {
    // scalar
    uint64_t shiftAmount = Src2.IntVal.getZExtValue();
    llvm::APInt valueToShift = Src1.IntVal;
    Dest.IntVal = valueToShift.shl(getShiftAmount(shiftAmount, valueToShift));
  }

  SetValue(&I, Dest);
}

void InstrsInterpreter::CFAVisitLShr(BinaryOperator &I) {
  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue Dest;
  Type *Ty = I.getType();

  if (Ty->isVectorTy()) {
    uint32_t src1Size = uint32_t(Src1.AggregateVal.size());
    assert(src1Size == Src2.AggregateVal.size());
    for (unsigned i = 0; i < src1Size; i++) {
      GenericValue Result;
      uint64_t shiftAmount = Src2.AggregateVal[i].IntVal.getZExtValue();
      llvm::APInt valueToShift = Src1.AggregateVal[i].IntVal;
      Result.IntVal = valueToShift.lshr(getShiftAmount(shiftAmount, valueToShift));
      Dest.AggregateVal.push_back(Result);
    }
  } else {
    // scalar
    uint64_t shiftAmount = Src2.IntVal.getZExtValue();
    llvm::APInt valueToShift = Src1.IntVal;
    Dest.IntVal = valueToShift.lshr(getShiftAmount(shiftAmount, valueToShift));
  }

  SetValue(&I, Dest);  
}

void InstrsInterpreter::CFAVisitAShr(BinaryOperator &I) {
  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue Dest;
  Type *Ty = I.getType();

  if (Ty->isVectorTy()) {
    size_t src1Size = Src1.AggregateVal.size();
    assert(src1Size == Src2.AggregateVal.size());
    for (unsigned i = 0; i < src1Size; i++) {
      GenericValue Result;
      uint64_t shiftAmount = Src2.AggregateVal[i].IntVal.getZExtValue();
      llvm::APInt valueToShift = Src1.AggregateVal[i].IntVal;
      Result.IntVal = valueToShift.ashr(getShiftAmount(shiftAmount, valueToShift));
      Dest.AggregateVal.push_back(Result);
    }
  } else {
    // scalar
    uint64_t shiftAmount = Src2.IntVal.getZExtValue();
    llvm::APInt valueToShift = Src1.IntVal;
    Dest.IntVal = valueToShift.ashr(getShiftAmount(shiftAmount, valueToShift));
  }

  SetValue(&I, Dest);
}

void InstrsInterpreter::CFAVisitVAArgInst(VAArgInst &I) {
  // In CFA execution mode, we provide a simplified implementation
  // that creates a default value for the requested type
  GenericValue Dest;
  Type *Ty = I.getType();
  switch (Ty->getTypeID()) {
  case Type::IntegerTyID:
    Dest.IntVal = APInt(Ty->getIntegerBitWidth(), 0);
    break;
  case Type::PointerTyID:
    Dest.PointerVal = nullptr;
    break;
  case Type::FloatTyID:
    Dest.FloatVal = 0.0f;
    break;
  case Type::DoubleTyID:
    Dest.DoubleVal = 0.0;
    break;
  default:
    dbgs() << "Unhandled dest type for vaarg instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }

  // Set the Value of this Instruction.
  SetValue(&I, Dest);
}

void InstrsInterpreter::CFAVisitExtractElementInst(ExtractElementInst &I) {
  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue Dest;

  Type *Ty = I.getType();
  const unsigned indx = unsigned(Src2.IntVal.getZExtValue());

  if(Src1.AggregateVal.size() > indx) {
    switch (Ty->getTypeID()) {
    default:
      dbgs() << "Unhandled destination type for extractelement instruction: "
      << *Ty << "\n";
      llvm_unreachable(nullptr);
      break;
    case Type::IntegerTyID:
      Dest.IntVal = Src1.AggregateVal[indx].IntVal;
      break;
    case Type::FloatTyID:
      Dest.FloatVal = Src1.AggregateVal[indx].FloatVal;
      break;
    case Type::DoubleTyID:
      Dest.DoubleVal = Src1.AggregateVal[indx].DoubleVal;
      break;
    }
  } else {
    dbgs() << "Invalid index in extractelement instruction\n";
  }

  SetValue(&I, Dest);
}

void InstrsInterpreter::CFAVisitInsertElementInst(InsertElementInst &I) {
  VectorType *Ty = cast<VectorType>(I.getType());

  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue Src3 = getOperandValue(I.getOperand(2));
  GenericValue Dest;

  Type *TyContained = Ty->getElementType();

  const unsigned indx = unsigned(Src3.IntVal.getZExtValue());
  Dest.AggregateVal = Src1.AggregateVal;

  if(Src1.AggregateVal.size() <= indx)
      llvm_unreachable("Invalid index in insertelement instruction");
  switch (TyContained->getTypeID()) {
    default:
      llvm_unreachable("Unhandled dest type for insertelement instruction");
    case Type::IntegerTyID:
      Dest.AggregateVal[indx].IntVal = Src2.IntVal;
      break;
    case Type::FloatTyID:
      Dest.AggregateVal[indx].FloatVal = Src2.FloatVal;
      break;
    case Type::DoubleTyID:
      Dest.AggregateVal[indx].DoubleVal = Src2.DoubleVal;
      break;
  }
  SetValue(&I, Dest);
}

void InstrsInterpreter::CFAVisitShuffleVectorInst(ShuffleVectorInst &I) {
  VectorType *Ty = cast<VectorType>(I.getType());

  GenericValue Src1 = getOperandValue(I.getOperand(0));
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue Dest;

  // There is no need to check types of src1 and src2, because the compiled
  // bytecode can't contain different types for src1 and src2 for a
  // shufflevector instruction.

  Type *TyContained = Ty->getElementType();
  unsigned src1Size = (unsigned)Src1.AggregateVal.size();
  unsigned src2Size = (unsigned)Src2.AggregateVal.size();
  unsigned src3Size = I.getShuffleMask().size();

  Dest.AggregateVal.resize(src3Size);

  switch (TyContained->getTypeID()) {
    default:
      llvm_unreachable("Unhandled dest type for insertelement instruction");
      break;
    case Type::IntegerTyID:
      for( unsigned i=0; i<src3Size; i++) {
        unsigned j = std::max(0, I.getMaskValue(i));
        if(j < src1Size)
          Dest.AggregateVal[i].IntVal = Src1.AggregateVal[j].IntVal;
        else if(j < src1Size + src2Size)
          Dest.AggregateVal[i].IntVal = Src2.AggregateVal[j-src1Size].IntVal;
        else
          // The selector may not be greater than sum of lengths of first and
          // second operands and llasm should not allow situation like
          // %tmp = shufflevector <2 x i32> <i32 3, i32 4>, <2 x i32> undef,
          //                      <2 x i32> < i32 0, i32 5 >,
          // where i32 5 is invalid, but let it be additional check here:
          llvm_unreachable("Invalid mask in shufflevector instruction");
      }
      break;
    case Type::FloatTyID:
      for( unsigned i=0; i<src3Size; i++) {
        unsigned j = std::max(0, I.getMaskValue(i));
        if(j < src1Size)
          Dest.AggregateVal[i].FloatVal = Src1.AggregateVal[j].FloatVal;
        else if(j < src1Size + src2Size)
          Dest.AggregateVal[i].FloatVal = Src2.AggregateVal[j-src1Size].FloatVal;
        else
          llvm_unreachable("Invalid mask in shufflevector instruction");
        }
      break;
    case Type::DoubleTyID:
      for( unsigned i=0; i<src3Size; i++) {
        unsigned j = std::max(0, I.getMaskValue(i));
        if(j < src1Size)
          Dest.AggregateVal[i].DoubleVal = Src1.AggregateVal[j].DoubleVal;
        else if(j < src1Size + src2Size)
          Dest.AggregateVal[i].DoubleVal =
            Src2.AggregateVal[j-src1Size].DoubleVal;
        else
          llvm_unreachable("Invalid mask in shufflevector instruction");
      }
      break;
  }
  SetValue(&I, Dest); 
}

void InstrsInterpreter::CFAVisitExtractValueInst(ExtractValueInst &I) {
  Value *Agg = I.getAggregateOperand();
  GenericValue Dest;
  GenericValue Src = getOperandValue(Agg);

  ExtractValueInst::idx_iterator IdxBegin = I.idx_begin();
  unsigned Num = I.getNumIndices();
  GenericValue *pSrc = &Src;

  for (unsigned i = 0 ; i < Num; ++i) {
    pSrc = &pSrc->AggregateVal[*IdxBegin];
    ++IdxBegin;
  }

  Type *IndexedType = ExtractValueInst::getIndexedType(Agg->getType(), I.getIndices());
  switch (IndexedType->getTypeID()) {
    default:
      llvm_unreachable("Unhandled dest type for extractelement instruction");
    break;
    case Type::IntegerTyID:
      Dest.IntVal = pSrc->IntVal;
    break;
    case Type::FloatTyID:
      Dest.FloatVal = pSrc->FloatVal;
    break;
    case Type::DoubleTyID:
      Dest.DoubleVal = pSrc->DoubleVal;
    break;
    case Type::ArrayTyID:
    case Type::StructTyID:
    case Type::FixedVectorTyID:
    case Type::ScalableVectorTyID:
      Dest.AggregateVal = pSrc->AggregateVal;
    break;
    case Type::PointerTyID:
      Dest.PointerVal = pSrc->PointerVal;
    break;
  }

  SetValue(&I, Dest);   
}

void InstrsInterpreter::CFAVisitInsertValueInst(InsertValueInst &I) {
  Value *Agg = I.getAggregateOperand();

  GenericValue Src1 = getOperandValue(Agg);
  GenericValue Src2 = getOperandValue(I.getOperand(1));
  GenericValue Dest = Src1; // Dest is a slightly changed Src1

  ExtractValueInst::idx_iterator IdxBegin = I.idx_begin();
  unsigned Num = I.getNumIndices();

  GenericValue *pDest = &Dest;
  for (unsigned i = 0 ; i < Num; ++i) {
    pDest = &pDest->AggregateVal[*IdxBegin];
    ++IdxBegin;
  }
  // pDest points to the target value in the Dest now

  Type *IndexedType = ExtractValueInst::getIndexedType(Agg->getType(), I.getIndices());

  switch (IndexedType->getTypeID()) {
    default:
      llvm_unreachable("Unhandled dest type for insertelement instruction");
    break;
    case Type::IntegerTyID:
      pDest->IntVal = Src2.IntVal;
    break;
    case Type::FloatTyID:
      pDest->FloatVal = Src2.FloatVal;
    break;
    case Type::DoubleTyID:
      pDest->DoubleVal = Src2.DoubleVal;
    break;
    case Type::ArrayTyID:
    case Type::StructTyID:
    case Type::FixedVectorTyID:
    case Type::ScalableVectorTyID:
      pDest->AggregateVal = Src2.AggregateVal;
    break;
    case Type::PointerTyID:
      pDest->PointerVal = Src2.PointerVal;
    break;
  }

  SetValue(&I, Dest); 
}

//===----------------------------------------------------------------------===//
//                    Instruction Execution Methods
//===----------------------------------------------------------------------===//

GenericValue InstrsInterpreter::executeGEPOperation(Value *Ptr, llvm::gep_type_iterator I,
                                                    llvm::gep_type_iterator E) {
  assert(Ptr->getType()->isPointerTy() &&
         "Cannot getElementOffset of a nonpointer type!");  

  uint64_t Total = 0;

  for (; I != E; ++I) {
    if (StructType *STy = I.getStructTypeOrNull()) {
      const StructLayout *SLO = getDataLayout().getStructLayout(STy);

      const ConstantInt *CPU = cast<ConstantInt>(I.getOperand());
      unsigned Index = unsigned(CPU->getZExtValue());

      Total += SLO->getElementOffset(Index);
    } else {
      // Get the index number for the array... which must be long type...
      GenericValue IdxGV = getOperandValue(I.getOperand());

      int64_t Idx;
      unsigned BitWidth =
        cast<IntegerType>(I.getOperand()->getType())->getBitWidth();
      if (BitWidth == 32)
        Idx = (int64_t)(int32_t)IdxGV.IntVal.getZExtValue();
      else {
        assert(BitWidth == 64 && "Invalid index type for getelementptr");
        Idx = (int64_t)IdxGV.IntVal.getZExtValue();
      }
      Total += getDataLayout().getTypeAllocSize(I.getIndexedType()) * Idx;
    }
  }

  GenericValue Result;
  Result.PointerVal = ((char*)getOperandValue(Ptr).PointerVal) + Total;
  LLVM_DEBUG(dbgs() << "GEP Index " << Total << " bytes.\n");
  return Result;
}

GenericValue InstrsInterpreter::getConstantExprValue(ConstantExpr *CE) {
  switch (CE->getOpcode()) {
    case Instruction::Trunc:
        return executeTruncInst(CE->getOperand(0), CE->getType());
    case Instruction::ZExt:
        return executeZExtInst(CE->getOperand(0), CE->getType());
    case Instruction::SExt:
        return executeSExtInst(CE->getOperand(0), CE->getType());
    case Instruction::FPTrunc:
        return executeFPTruncInst(CE->getOperand(0), CE->getType());
    case Instruction::FPExt:
        return executeFPExtInst(CE->getOperand(0), CE->getType());
    case Instruction::UIToFP:
        return executeUIToFPInst(CE->getOperand(0), CE->getType());
    case Instruction::SIToFP:
        return executeSIToFPInst(CE->getOperand(0), CE->getType());
    case Instruction::FPToUI:
        return executeFPToUIInst(CE->getOperand(0), CE->getType());
    case Instruction::FPToSI:
        return executeFPToSIInst(CE->getOperand(0), CE->getType());
    case Instruction::PtrToInt:
        return executePtrToIntInst(CE->getOperand(0), CE->getType());
    case Instruction::IntToPtr:
        return executeIntToPtrInst(CE->getOperand(0), CE->getType());
    case Instruction::BitCast:
        return executeBitCastInst(CE->getOperand(0), CE->getType());
    case Instruction::GetElementPtr:
      return executeGEPOperation(CE->getOperand(0), gep_type_begin(CE),
                                 gep_type_end(CE));
    case Instruction::FCmp:
    case Instruction::ICmp:
      return executeCmpInst(CE->getPredicate(),
                            getOperandValue(CE->getOperand(0)),
                            getOperandValue(CE->getOperand(1)),
                            CE->getOperand(0)->getType());
    case Instruction::Select:
      return executeSelectInst(getOperandValue(CE->getOperand(0)),
                               getOperandValue(CE->getOperand(1)),
                               getOperandValue(CE->getOperand(2)),
                               CE->getOperand(0)->getType());
    default :
      break;
    }
  
    // The cases below here require a GenericValue parameter for the result
    // so we initialize one, compute it and then return it.
    GenericValue Op0 = getOperandValue(CE->getOperand(0));
    GenericValue Op1 = getOperandValue(CE->getOperand(1));
    GenericValue Dest;
    Type * Ty = CE->getOperand(0)->getType();
    switch (CE->getOpcode()) {
    case Instruction::Add:  Dest.IntVal = Op0.IntVal + Op1.IntVal; break;
    case Instruction::Sub:  Dest.IntVal = Op0.IntVal - Op1.IntVal; break;
    case Instruction::Mul:  Dest.IntVal = Op0.IntVal * Op1.IntVal; break;
    case Instruction::FAdd: executeFAddInst(Dest, Op0, Op1, Ty); break;
    case Instruction::FSub: executeFSubInst(Dest, Op0, Op1, Ty); break;
    case Instruction::FMul: executeFMulInst(Dest, Op0, Op1, Ty); break;
    case Instruction::FDiv: executeFDivInst(Dest, Op0, Op1, Ty); break;
    case Instruction::FRem: executeFRemInst(Dest, Op0, Op1, Ty); break;
    case Instruction::SDiv: Dest.IntVal = Op0.IntVal.sdiv(Op1.IntVal); break;
    case Instruction::UDiv: Dest.IntVal = Op0.IntVal.udiv(Op1.IntVal); break;
    case Instruction::URem: Dest.IntVal = Op0.IntVal.urem(Op1.IntVal); break;
    case Instruction::SRem: Dest.IntVal = Op0.IntVal.srem(Op1.IntVal); break;
    case Instruction::And:  Dest.IntVal = Op0.IntVal & Op1.IntVal; break;
    case Instruction::Or:   Dest.IntVal = Op0.IntVal | Op1.IntVal; break;
    case Instruction::Xor:  Dest.IntVal = Op0.IntVal ^ Op1.IntVal; break;
    case Instruction::Shl:
      Dest.IntVal = Op0.IntVal.shl(Op1.IntVal.getZExtValue());
      break;
    case Instruction::LShr:
      Dest.IntVal = Op0.IntVal.lshr(Op1.IntVal.getZExtValue());
      break;
    case Instruction::AShr:
      Dest.IntVal = Op0.IntVal.ashr(Op1.IntVal.getZExtValue());
      break;
    default:
      dbgs() << "Unhandled ConstantExpr: " << *CE << "\n";
      llvm_unreachable("Unhandled ConstantExpr");
    }
    return Dest;
}

GenericValue InstrsInterpreter::getOperandValue(Value *V) {
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(V)) {
    return getConstantExprValue(CE);
  } else if (Constant *CPV = dyn_cast<Constant>(V)) {
    return getConstantValue(CPV);
  } else if (GlobalValue *GV = dyn_cast<GlobalValue>(V)) {
    return PTOGV(getPointerToGlobal(GV));
  } else {
    return cfaContext.Values[V];
  }
}

GenericValue InstrsInterpreter::executeTruncInst(Value *SrcVal, Type *DstTy) {
  GenericValue Dest, Src = getOperandValue(SrcVal);
  Type *SrcTy = SrcVal->getType();
  if (SrcTy->isVectorTy()) {
    Type *DstVecTy = DstTy->getScalarType();
    unsigned DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();
    unsigned NumElts = Src.AggregateVal.size();
    // the sizes of src and dst vectors must be equal
    Dest.AggregateVal.resize(NumElts);
    for (unsigned i = 0; i < NumElts; i++)
      Dest.AggregateVal[i].IntVal = Src.AggregateVal[i].IntVal.trunc(DBitWidth);
  } else {
    IntegerType *DITy = cast<IntegerType>(DstTy);
    unsigned DBitWidth = DITy->getBitWidth();
    Dest.IntVal = Src.IntVal.trunc(DBitWidth);
  }
  return Dest;
}

GenericValue InstrsInterpreter::executeSExtInst(Value *SrcVal, Type *DstTy) {
  Type *SrcTy = SrcVal->getType();
  GenericValue Dest, Src = getOperandValue(SrcVal);
  if (SrcTy->isVectorTy()) {
    Type *DstVecTy = DstTy->getScalarType();
    unsigned DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();
    unsigned size = Src.AggregateVal.size();
    // the sizes of src and dst vectors must be equal.
    Dest.AggregateVal.resize(size);
    for (unsigned i = 0; i < size; i++)
      Dest.AggregateVal[i].IntVal = Src.AggregateVal[i].IntVal.sext(DBitWidth);
  } else {
    auto *DITy = cast<IntegerType>(DstTy);
    unsigned DBitWidth = DITy->getBitWidth();
    Dest.IntVal = Src.IntVal.sext(DBitWidth);
  }
  return Dest;
}

GenericValue InstrsInterpreter::executeZExtInst(Value *SrcVal, Type *DstTy) {
  Type *SrcTy = SrcVal->getType();
  GenericValue Dest, Src = getOperandValue(SrcVal);
  if (SrcTy->isVectorTy()) {
    Type *DstVecTy = DstTy->getScalarType();
    unsigned DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();

    unsigned size = Src.AggregateVal.size();
    // the sizes of src and dst vectors must be equal.
    Dest.AggregateVal.resize(size);
    for (unsigned i = 0; i < size; i++)
      Dest.AggregateVal[i].IntVal = Src.AggregateVal[i].IntVal.zext(DBitWidth);
  } else {
    auto *DITy = cast<IntegerType>(DstTy);
    unsigned DBitWidth = DITy->getBitWidth();
    Dest.IntVal = Src.IntVal.zext(DBitWidth);
  }
  return Dest;  
}

GenericValue InstrsInterpreter::executeFPTruncInst(Value *SrcVal, Type *DstTy) {
  GenericValue Dest, Src = getOperandValue(SrcVal);

  if (isa<VectorType>(SrcVal->getType())) {
    assert(SrcVal->getType()->getScalarType()->isDoubleTy() &&
           DstTy->getScalarType()->isFloatTy() &&
           "Invalid FPTrunc instruction");

    unsigned size = Src.AggregateVal.size();
    // the sizes of src and dst vectors must be equal.
    Dest.AggregateVal.resize(size);
    for (unsigned i = 0; i < size; i++)
      Dest.AggregateVal[i].FloatVal = (float)Src.AggregateVal[i].DoubleVal;
  } else {
    assert(SrcVal->getType()->isDoubleTy() && DstTy->isFloatTy() &&
           "Invalid FPTrunc instruction");
    Dest.FloatVal = (float)Src.DoubleVal;
  }

  return Dest;
}

GenericValue InstrsInterpreter::executeFPExtInst(Value *SrcVal, Type *DstTy) {
  GenericValue Dest, Src = getOperandValue(SrcVal);

  if (isa<VectorType>(SrcVal->getType())) {
    assert(SrcVal->getType()->getScalarType()->isFloatTy() &&
           DstTy->getScalarType()->isDoubleTy() && "Invalid FPExt instruction");

    unsigned size = Src.AggregateVal.size();
    // the sizes of src and dst vectors must be equal.
    Dest.AggregateVal.resize(size);
    for (unsigned i = 0; i < size; i++)
      Dest.AggregateVal[i].DoubleVal = (double)Src.AggregateVal[i].FloatVal;
  } else {
    assert(SrcVal->getType()->isFloatTy() && DstTy->isDoubleTy() &&
           "Invalid FPExt instruction");
    Dest.DoubleVal = (double)Src.FloatVal;
  }

  return Dest;   
}

GenericValue InstrsInterpreter::executeFPToUIInst(Value *SrcVal, Type *DstTy) {
  Type *SrcTy = SrcVal->getType();
  GenericValue Dest, Src = getOperandValue(SrcVal);

  if (isa<VectorType>(SrcTy)) {
    Type *DstVecTy = DstTy->getScalarType();
    Type *SrcVecTy = SrcTy->getScalarType();
    uint32_t DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();
    unsigned size = Src.AggregateVal.size();
    // the sizes of src and dst vectors must be equal.
    Dest.AggregateVal.resize(size);

    if (SrcVecTy->getTypeID() == Type::FloatTyID) {
      assert(SrcVecTy->isFloatingPointTy() && "Invalid FPToUI instruction");
      for (unsigned i = 0; i < size; i++)
        Dest.AggregateVal[i].IntVal = APIntOps::RoundFloatToAPInt(
            Src.AggregateVal[i].FloatVal, DBitWidth);
    } else {
      for (unsigned i = 0; i < size; i++)
        Dest.AggregateVal[i].IntVal = APIntOps::RoundDoubleToAPInt(
            Src.AggregateVal[i].DoubleVal, DBitWidth);
    }
  } else {
    // scalar
    uint32_t DBitWidth = cast<IntegerType>(DstTy)->getBitWidth();
    assert(SrcTy->isFloatingPointTy() && "Invalid FPToUI instruction");

    if (SrcTy->getTypeID() == Type::FloatTyID)
      Dest.IntVal = APIntOps::RoundFloatToAPInt(Src.FloatVal, DBitWidth);
    else {
      Dest.IntVal = APIntOps::RoundDoubleToAPInt(Src.DoubleVal, DBitWidth);
    }
  }

  return Dest; 
}

GenericValue InstrsInterpreter::executeFPToSIInst(Value *SrcVal, Type *DstTy) {
  Type *SrcTy = SrcVal->getType();
  GenericValue Dest, Src = getOperandValue(SrcVal);

  if (isa<VectorType>(SrcTy)) {
    Type *DstVecTy = DstTy->getScalarType();
    Type *SrcVecTy = SrcTy->getScalarType();
    uint32_t DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();
    unsigned size = Src.AggregateVal.size();
    // the sizes of src and dst vectors must be equal
    Dest.AggregateVal.resize(size);

    if (SrcVecTy->getTypeID() == Type::FloatTyID) {
      assert(SrcVecTy->isFloatingPointTy() && "Invalid FPToSI instruction");
      for (unsigned i = 0; i < size; i++)
        Dest.AggregateVal[i].IntVal = APIntOps::RoundFloatToAPInt(
            Src.AggregateVal[i].FloatVal, DBitWidth);
    } else {
      for (unsigned i = 0; i < size; i++)
        Dest.AggregateVal[i].IntVal = APIntOps::RoundDoubleToAPInt(
            Src.AggregateVal[i].DoubleVal, DBitWidth);
    }
  } else {
    // scalar
    unsigned DBitWidth = cast<IntegerType>(DstTy)->getBitWidth();
    assert(SrcTy->isFloatingPointTy() && "Invalid FPToSI instruction");

    if (SrcTy->getTypeID() == Type::FloatTyID)
      Dest.IntVal = APIntOps::RoundFloatToAPInt(Src.FloatVal, DBitWidth);
    else {
      Dest.IntVal = APIntOps::RoundDoubleToAPInt(Src.DoubleVal, DBitWidth);
    }
  }
  return Dest;
}

GenericValue InstrsInterpreter::executeUIToFPInst(Value *SrcVal, Type *DstTy) {
  GenericValue Dest, Src = getOperandValue(SrcVal);

  if (isa<VectorType>(SrcVal->getType())) {
    Type *DstVecTy = DstTy->getScalarType();
    unsigned size = Src.AggregateVal.size();
    // the sizes of src and dst vectors must be equal
    Dest.AggregateVal.resize(size);

    if (DstVecTy->getTypeID() == Type::FloatTyID) {
      assert(DstVecTy->isFloatingPointTy() && "Invalid UIToFP instruction");
      for (unsigned i = 0; i < size; i++)
        Dest.AggregateVal[i].FloatVal =
            APIntOps::RoundAPIntToFloat(Src.AggregateVal[i].IntVal);
    } else {
      for (unsigned i = 0; i < size; i++)
        Dest.AggregateVal[i].DoubleVal =
            APIntOps::RoundAPIntToDouble(Src.AggregateVal[i].IntVal);
    }
  } else {
    // scalar
    assert(DstTy->isFloatingPointTy() && "Invalid UIToFP instruction");
    if (DstTy->getTypeID() == Type::FloatTyID)
      Dest.FloatVal = APIntOps::RoundAPIntToFloat(Src.IntVal);
    else {
      Dest.DoubleVal = APIntOps::RoundAPIntToDouble(Src.IntVal);
    }
  }
  return Dest;
}

GenericValue InstrsInterpreter::executeSIToFPInst(Value *SrcVal, Type *DstTy) {
  GenericValue Dest, Src = getOperandValue(SrcVal);

  if (isa<VectorType>(SrcVal->getType())) {
    Type *DstVecTy = DstTy->getScalarType();
    unsigned size = Src.AggregateVal.size();
    // the sizes of src and dst vectors must be equal
    Dest.AggregateVal.resize(size);

    if (DstVecTy->getTypeID() == Type::FloatTyID) {
      assert(DstVecTy->isFloatingPointTy() && "Invalid SIToFP instruction");
      for (unsigned i = 0; i < size; i++)
        Dest.AggregateVal[i].FloatVal =
            APIntOps::RoundSignedAPIntToFloat(Src.AggregateVal[i].IntVal);
    } else {
      for (unsigned i = 0; i < size; i++)
        Dest.AggregateVal[i].DoubleVal =
            APIntOps::RoundSignedAPIntToDouble(Src.AggregateVal[i].IntVal);
    }
  } else {
    // scalar
    assert(DstTy->isFloatingPointTy() && "Invalid SIToFP instruction");

    if (DstTy->getTypeID() == Type::FloatTyID)
      Dest.FloatVal = APIntOps::RoundSignedAPIntToFloat(Src.IntVal);
    else {
      Dest.DoubleVal = APIntOps::RoundSignedAPIntToDouble(Src.IntVal);
    }
  }

  return Dest;   
}

GenericValue InstrsInterpreter::executePtrToIntInst(Value *SrcVal, Type *DstTy) {
  uint32_t DBitWidth = cast<IntegerType>(DstTy)->getBitWidth();
  GenericValue Dest, Src = getOperandValue(SrcVal);
  assert(SrcVal->getType()->isPointerTy() && "Invalid PtrToInt instruction");

  Dest.IntVal = APInt(DBitWidth, (intptr_t) Src.PointerVal);
  return Dest;   
}

GenericValue InstrsInterpreter::executeIntToPtrInst(Value *SrcVal, Type *DstTy) {
  GenericValue Dest, Src = getOperandValue(SrcVal);
  assert(DstTy->isPointerTy() && "Invalid PtrToInt instruction");

  uint32_t PtrSize = getDataLayout().getPointerSizeInBits();
  if (PtrSize != Src.IntVal.getBitWidth())
    Src.IntVal = Src.IntVal.zextOrTrunc(PtrSize);

  Dest.PointerVal = PointerTy(intptr_t(Src.IntVal.getZExtValue()));
  return Dest;    
}

GenericValue InstrsInterpreter::executeBitCastInst(Value *SrcVal, Type *DstTy) {
  // This instruction supports bitwise conversion of vectors to integers and
  // to vectors of other types (as long as they have the same size)
  Type *SrcTy = SrcVal->getType();
  GenericValue Dest, Src = getOperandValue(SrcVal);

  if (isa<VectorType>(SrcTy) || isa<VectorType>(DstTy)) {
    // vector src bitcast to vector dst or vector src bitcast to scalar dst or
    // scalar src bitcast to vector dst
    bool isLittleEndian = getDataLayout().isLittleEndian();
    GenericValue TempDst, TempSrc, SrcVec;
    Type *SrcElemTy;
    Type *DstElemTy;
    unsigned SrcBitSize;
    unsigned DstBitSize;
    unsigned SrcNum;
    unsigned DstNum;

    if (isa<VectorType>(SrcTy)) {
      SrcElemTy = SrcTy->getScalarType();
      SrcBitSize = SrcTy->getScalarSizeInBits();
      SrcNum = Src.AggregateVal.size();
      SrcVec = Src;
    } else {
      // if src is scalar value, make it vector <1 x type>
      SrcElemTy = SrcTy;
      SrcBitSize = SrcTy->getPrimitiveSizeInBits();
      SrcNum = 1;
      SrcVec.AggregateVal.push_back(Src);
    }

    if (isa<VectorType>(DstTy)) {
      DstElemTy = DstTy->getScalarType();
      DstBitSize = DstTy->getScalarSizeInBits();
      DstNum = (SrcNum * SrcBitSize) / DstBitSize;
    } else {
      DstElemTy = DstTy;
      DstBitSize = DstTy->getPrimitiveSizeInBits();
      DstNum = 1;
    }

    if (SrcNum * SrcBitSize != DstNum * DstBitSize)
      llvm_unreachable("Invalid BitCast");

    // If src is floating point, cast to integer first.
    TempSrc.AggregateVal.resize(SrcNum);
    if (SrcElemTy->isFloatTy()) {
      for (unsigned i = 0; i < SrcNum; i++)
        TempSrc.AggregateVal[i].IntVal =
            APInt::floatToBits(SrcVec.AggregateVal[i].FloatVal);

    } else if (SrcElemTy->isDoubleTy()) {
      for (unsigned i = 0; i < SrcNum; i++)
        TempSrc.AggregateVal[i].IntVal =
            APInt::doubleToBits(SrcVec.AggregateVal[i].DoubleVal);
    } else if (SrcElemTy->isIntegerTy()) {
      for (unsigned i = 0; i < SrcNum; i++)
        TempSrc.AggregateVal[i].IntVal = SrcVec.AggregateVal[i].IntVal;
    } else {
      // Pointers are not allowed as the element type of vector.
      llvm_unreachable("Invalid Bitcast");
    }

    // now TempSrc is integer type vector
    if (DstNum < SrcNum) {
      // Example: bitcast <4 x i32> <i32 0, i32 1, i32 2, i32 3> to <2 x i64>
      unsigned Ratio = SrcNum / DstNum;
      unsigned SrcElt = 0;
      for (unsigned i = 0; i < DstNum; i++) {
        GenericValue Elt;
        Elt.IntVal = 0;
        Elt.IntVal = Elt.IntVal.zext(DstBitSize);
        unsigned ShiftAmt = isLittleEndian ? 0 : SrcBitSize * (Ratio - 1);
        for (unsigned j = 0; j < Ratio; j++) {
          APInt Tmp;
          Tmp = Tmp.zext(SrcBitSize);
          Tmp = TempSrc.AggregateVal[SrcElt++].IntVal;
          Tmp = Tmp.zext(DstBitSize);
          Tmp <<= ShiftAmt;
          ShiftAmt += isLittleEndian ? SrcBitSize : -SrcBitSize;
          Elt.IntVal |= Tmp;
        }
        TempDst.AggregateVal.push_back(Elt);
      }
    } else {
      // Example: bitcast <2 x i64> <i64 0, i64 1> to <4 x i32>
      unsigned Ratio = DstNum / SrcNum;
      for (unsigned i = 0; i < SrcNum; i++) {
        unsigned ShiftAmt = isLittleEndian ? 0 : DstBitSize * (Ratio - 1);
        for (unsigned j = 0; j < Ratio; j++) {
          GenericValue Elt;
          Elt.IntVal = Elt.IntVal.zext(SrcBitSize);
          Elt.IntVal = TempSrc.AggregateVal[i].IntVal;
          Elt.IntVal.lshrInPlace(ShiftAmt);
          // it could be DstBitSize == SrcBitSize, so check it
          if (DstBitSize < SrcBitSize)
            Elt.IntVal = Elt.IntVal.trunc(DstBitSize);
          ShiftAmt += isLittleEndian ? DstBitSize : -DstBitSize;
          TempDst.AggregateVal.push_back(Elt);
        }
      }
    }

    // convert result from integer to specified type
    if (isa<VectorType>(DstTy)) {
      if (DstElemTy->isDoubleTy()) {
        Dest.AggregateVal.resize(DstNum);
        for (unsigned i = 0; i < DstNum; i++)
          Dest.AggregateVal[i].DoubleVal =
              TempDst.AggregateVal[i].IntVal.bitsToDouble();
      } else if (DstElemTy->isFloatTy()) {
        Dest.AggregateVal.resize(DstNum);
        for (unsigned i = 0; i < DstNum; i++)
          Dest.AggregateVal[i].FloatVal =
              TempDst.AggregateVal[i].IntVal.bitsToFloat();
      } else {
        Dest = TempDst;
      }
    } else {
      if (DstElemTy->isDoubleTy())
        Dest.DoubleVal = TempDst.AggregateVal[0].IntVal.bitsToDouble();
      else if (DstElemTy->isFloatTy()) {
        Dest.FloatVal = TempDst.AggregateVal[0].IntVal.bitsToFloat();
      } else {
        Dest.IntVal = TempDst.AggregateVal[0].IntVal;
      }
    }
  } else { //  if (isa<VectorType>(SrcTy)) || isa<VectorType>(DstTy))

    // scalar src bitcast to scalar dst
    if (DstTy->isPointerTy()) {
      assert(SrcTy->isPointerTy() && "Invalid BitCast");
      Dest.PointerVal = Src.PointerVal;
    } else if (DstTy->isIntegerTy()) {
      if (SrcTy->isFloatTy())
        Dest.IntVal = APInt::floatToBits(Src.FloatVal);
      else if (SrcTy->isDoubleTy()) {
        Dest.IntVal = APInt::doubleToBits(Src.DoubleVal);
      } else if (SrcTy->isIntegerTy()) {
        Dest.IntVal = Src.IntVal;
      } else {
        llvm_unreachable("Invalid BitCast");
      }
    } else if (DstTy->isFloatTy()) {
      if (SrcTy->isIntegerTy())
        Dest.FloatVal = Src.IntVal.bitsToFloat();
      else {
        Dest.FloatVal = Src.FloatVal;
      }
    } else if (DstTy->isDoubleTy()) {
      if (SrcTy->isIntegerTy())
        Dest.DoubleVal = Src.IntVal.bitsToDouble();
      else {
        Dest.DoubleVal = Src.DoubleVal;
      }
    } else {
      llvm_unreachable("Invalid Bitcast");
    }
  }

  return Dest;   
}

// GenericValue InstrsInterpreter::executeCastOperation(Instruction::CastOps opcode, Value *SrcVal, Type *Ty) {
//   // Why Execution.cpp don't have these implementations?
// }

void InstrsInterpreter::executeFNegInst(GenericValue &Dest, GenericValue Src, Type *Ty) {
    switch (Ty->getTypeID()) {
    case Type::FloatTyID:
      Dest.FloatVal = -Src.FloatVal;
      break;
    case Type::DoubleTyID:
      Dest.DoubleVal = -Src.DoubleVal;
      break;
    default:
      llvm_unreachable("Unhandled type for FNeg instruction");
    }
}

#define IMPLEMENT_BINARY_OPERATOR(OP, TY) \
   case Type::TY##TyID: \
     Dest.TY##Val = Src1.TY##Val OP Src2.TY##Val; \
     break

void InstrsInterpreter::executeFAddInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
    IMPLEMENT_BINARY_OPERATOR(+, Float);
    IMPLEMENT_BINARY_OPERATOR(+, Double);
  default:
    dbgs() << "Unhandled type for FAdd instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }        
}

void InstrsInterpreter::executeFSubInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
    IMPLEMENT_BINARY_OPERATOR(-, Float);
    IMPLEMENT_BINARY_OPERATOR(-, Double);
  default:
    dbgs() << "Unhandled type for FSub instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
}

void InstrsInterpreter::executeFMulInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
    IMPLEMENT_BINARY_OPERATOR(*, Float);
    IMPLEMENT_BINARY_OPERATOR(*, Double);
  default:
    dbgs() << "Unhandled type for FMul instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }  
}

void InstrsInterpreter::executeFDivInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
    IMPLEMENT_BINARY_OPERATOR(/, Float);
    IMPLEMENT_BINARY_OPERATOR(/, Double);
  default:
    dbgs() << "Unhandled type for FDiv instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }   
}

void InstrsInterpreter::executeFRemInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
  case Type::FloatTyID:
    Dest.FloatVal = fmod(Src1.FloatVal, Src2.FloatVal);
    break;
  case Type::DoubleTyID:
    Dest.DoubleVal = fmod(Src1.DoubleVal, Src2.DoubleVal);
    break;
  default:
    dbgs() << "Unhandled type for Rem instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }   
}

#define IMPLEMENT_INTEGER_ICMP(OP, TY) \
   case Type::IntegerTyID:  \
      Dest.IntVal = APInt(1,Src1.IntVal.OP(Src2.IntVal)); \
      break;

#define IMPLEMENT_VECTOR_INTEGER_ICMP(OP, TY)                                  \
  case Type::FixedVectorTyID:                                                  \
  case Type::ScalableVectorTyID: {                                             \
    assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());              \
    Dest.AggregateVal.resize(Src1.AggregateVal.size());                        \
    for (uint32_t _i = 0; _i < Src1.AggregateVal.size(); _i++)                 \
      Dest.AggregateVal[_i].IntVal = APInt(                                    \
          1, Src1.AggregateVal[_i].IntVal.OP(Src2.AggregateVal[_i].IntVal));   \
  } break;

// Handle pointers specially because they must be compared with only as much
// width as the host has.  We _do not_ want to be comparing 64 bit values when
// running on a 32-bit target, otherwise the upper 32 bits might mess up
// comparisons if they contain garbage.
#define IMPLEMENT_POINTER_ICMP(OP) \
   case Type::PointerTyID: \
      Dest.IntVal = APInt(1,(void*)(intptr_t)Src1.PointerVal OP \
                            (void*)(intptr_t)Src2.PointerVal); \
      break;

GenericValue InstrsInterpreter::executeICMP_EQ(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(eq,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(eq,Ty);
    IMPLEMENT_POINTER_ICMP(==);
  default:
    dbgs() << "Unhandled type for ICMP_EQ predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;   
}

GenericValue InstrsInterpreter::executeICMP_NE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  // llvm::outs() << "Src1: " << Src1.PointerVal << "\n";
  // llvm::outs() << "Src2: " << Src2.PointerVal << "\n";
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(ne,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(ne,Ty);
    IMPLEMENT_POINTER_ICMP(!=);
  default:
    dbgs() << "Unhandled type for ICMP_NE predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  // llvm::outs() << "Dest: " << Dest.IntVal << "\n";
  return Dest;   
}

GenericValue InstrsInterpreter::executeICMP_ULT(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(ult,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(ult,Ty);
    IMPLEMENT_POINTER_ICMP(<);
  default:
    dbgs() << "Unhandled type for ICMP_ULT predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;    
}

GenericValue InstrsInterpreter::executeICMP_SLT(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(slt,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(slt,Ty);
    IMPLEMENT_POINTER_ICMP(<);
  default:
    dbgs() << "Unhandled type for ICMP_SLT predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;    
}

GenericValue InstrsInterpreter::executeICMP_UGT(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(ugt,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(ugt,Ty);
    IMPLEMENT_POINTER_ICMP(>);
  default:
    dbgs() << "Unhandled type for ICMP_UGT predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;   
}

GenericValue InstrsInterpreter::executeICMP_SGT(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(sgt,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(sgt,Ty);
    IMPLEMENT_POINTER_ICMP(>);
  default:
    dbgs() << "Unhandled type for ICMP_SGT predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;   
}

GenericValue InstrsInterpreter::executeICMP_ULE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(ule,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(ule,Ty);
    IMPLEMENT_POINTER_ICMP(<=);
  default:
    dbgs() << "Unhandled type for ICMP_ULE predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;    
}

GenericValue InstrsInterpreter::executeICMP_SLE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(sle,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(sle,Ty);
    IMPLEMENT_POINTER_ICMP(<=);
  default:
    dbgs() << "Unhandled type for ICMP_SLE predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;   
}

GenericValue InstrsInterpreter::executeICMP_UGE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(uge,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(uge,Ty);
    IMPLEMENT_POINTER_ICMP(>=);
  default:
    dbgs() << "Unhandled type for ICMP_UGE predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;    
}

GenericValue InstrsInterpreter::executeICMP_SGE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(sge,Ty);
    IMPLEMENT_VECTOR_INTEGER_ICMP(sge,Ty);
    IMPLEMENT_POINTER_ICMP(>=);
  default:
    dbgs() << "Unhandled type for ICMP_SGE predicate: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;    
}

#define IMPLEMENT_FCMP(OP, TY) \
   case Type::TY##TyID: \
     Dest.IntVal = APInt(1,Src1.TY##Val OP Src2.TY##Val); \
     break

#define IMPLEMENT_VECTOR_FCMP_T(OP, TY)                             \
  assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());     \
  Dest.AggregateVal.resize( Src1.AggregateVal.size() );             \
  for( uint32_t _i=0;_i<Src1.AggregateVal.size();_i++)              \
    Dest.AggregateVal[_i].IntVal = APInt(1,                         \
    Src1.AggregateVal[_i].TY##Val OP Src2.AggregateVal[_i].TY##Val);\
  break;

#define IMPLEMENT_VECTOR_FCMP(OP)                                              \
  case Type::FixedVectorTyID:                                                  \
  case Type::ScalableVectorTyID:                                               \
    if (cast<VectorType>(Ty)->getElementType()->isFloatTy()) {                 \
      IMPLEMENT_VECTOR_FCMP_T(OP, Float);                                      \
    } else {                                                                   \
      IMPLEMENT_VECTOR_FCMP_T(OP, Double);                                     \
    }

#define IMPLEMENT_SCALAR_NANS(TY, X,Y)                                      \
  if (TY->isFloatTy()) {                                                    \
    if (X.FloatVal != X.FloatVal || Y.FloatVal != Y.FloatVal) {             \
      Dest.IntVal = APInt(1,false);                                         \
      return Dest;                                                          \
    }                                                                       \
  } else {                                                                  \
    if (X.DoubleVal != X.DoubleVal || Y.DoubleVal != Y.DoubleVal) {         \
      Dest.IntVal = APInt(1,false);                                         \
      return Dest;                                                          \
    }                                                                       \
  }

#define MASK_VECTOR_NANS_T(X,Y, TZ, FLAG)                                   \
  assert(X.AggregateVal.size() == Y.AggregateVal.size());                   \
  Dest.AggregateVal.resize( X.AggregateVal.size() );                        \
  for( uint32_t _i=0;_i<X.AggregateVal.size();_i++) {                       \
    if (X.AggregateVal[_i].TZ##Val != X.AggregateVal[_i].TZ##Val ||         \
        Y.AggregateVal[_i].TZ##Val != Y.AggregateVal[_i].TZ##Val)           \
      Dest.AggregateVal[_i].IntVal = APInt(1,FLAG);                         \
    else  {                                                                 \
      Dest.AggregateVal[_i].IntVal = APInt(1,!FLAG);                        \
    }                                                                       \
  }

#define MASK_VECTOR_NANS(TY, X,Y, FLAG)                                     \
  if (TY->isVectorTy()) {                                                   \
    if (cast<VectorType>(TY)->getElementType()->isFloatTy()) {              \
      MASK_VECTOR_NANS_T(X, Y, Float, FLAG)                                 \
    } else {                                                                \
      MASK_VECTOR_NANS_T(X, Y, Double, FLAG)                                \
    }                                                                       \
  }

#define IMPLEMENT_UNORDERED(TY, X,Y)                                     \
  if (TY->isFloatTy()) {                                                 \
    if (X.FloatVal != X.FloatVal || Y.FloatVal != Y.FloatVal) {          \
      Dest.IntVal = APInt(1,true);                                       \
      return Dest;                                                       \
    }                                                                    \
  } else if (X.DoubleVal != X.DoubleVal || Y.DoubleVal != Y.DoubleVal) { \
    Dest.IntVal = APInt(1,true);                                         \
    return Dest;                                                         \
  }

#define IMPLEMENT_VECTOR_UNORDERED(TY, X, Y, FUNC)                             \
  if (TY->isVectorTy()) {                                                      \
    GenericValue DestMask = Dest;                                              \
    Dest = FUNC(Src1, Src2, Ty);                                               \
    for (size_t _i = 0; _i < Src1.AggregateVal.size(); _i++)                   \
      if (DestMask.AggregateVal[_i].IntVal == true)                            \
        Dest.AggregateVal[_i].IntVal = APInt(1, true);                         \
    return Dest;                                                               \
  }

GenericValue InstrsInterpreter::executeFCMP_OEQ(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(==, Float);
    IMPLEMENT_FCMP(==, Double);
    IMPLEMENT_VECTOR_FCMP(==);
  default:
    dbgs() << "Unhandled type for FCmp EQ instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;    
}

GenericValue InstrsInterpreter::executeFCMP_ONE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  // if input is scalar value and Src1 or Src2 is NaN return false
  IMPLEMENT_SCALAR_NANS(Ty, Src1, Src2)
  // if vector input detect NaNs and fill mask
  MASK_VECTOR_NANS(Ty, Src1, Src2, false)
  GenericValue DestMask = Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(!=, Float);
    IMPLEMENT_FCMP(!=, Double);
    IMPLEMENT_VECTOR_FCMP(!=);
    default:
      dbgs() << "Unhandled type for FCmp NE instruction: " << *Ty << "\n";
      llvm_unreachable(nullptr);
  }
  // in vector case mask out NaN elements
  if (Ty->isVectorTy())
    for( size_t _i=0; _i<Src1.AggregateVal.size(); _i++)
      if (DestMask.AggregateVal[_i].IntVal == false)
        Dest.AggregateVal[_i].IntVal = APInt(1,false);

  return Dest;    
}

GenericValue InstrsInterpreter::executeFCMP_OLE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(<=, Float);
    IMPLEMENT_FCMP(<=, Double);
    IMPLEMENT_VECTOR_FCMP(<=);
  default:
    dbgs() << "Unhandled type for FCmp LE instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;    
}

GenericValue InstrsInterpreter::executeFCMP_OGE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(>=, Float);
    IMPLEMENT_FCMP(>=, Double);
    IMPLEMENT_VECTOR_FCMP(>=);
  default:
    dbgs() << "Unhandled type for FCmp GE instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;   
}

GenericValue InstrsInterpreter::executeFCMP_OLT(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(<, Float);
    IMPLEMENT_FCMP(<, Double);
    IMPLEMENT_VECTOR_FCMP(<);
  default:
    dbgs() << "Unhandled type for FCmp LT instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;   
}

GenericValue InstrsInterpreter::executeFCMP_OGT(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(>, Float);
    IMPLEMENT_FCMP(>, Double);
    IMPLEMENT_VECTOR_FCMP(>);
  default:
    dbgs() << "Unhandled type for FCmp GT instruction: " << *Ty << "\n";
    llvm_unreachable(nullptr);
  }
  return Dest;    
}

GenericValue InstrsInterpreter::executeFCMP_UEQ(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  MASK_VECTOR_NANS(Ty, Src1, Src2, true)
  IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OEQ)
  return executeFCMP_OEQ(Src1, Src2, Ty);    
}

GenericValue InstrsInterpreter::executeFCMP_UNE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  MASK_VECTOR_NANS(Ty, Src1, Src2, true)
  IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_ONE)
  return executeFCMP_ONE(Src1, Src2, Ty);    
}

GenericValue InstrsInterpreter::executeFCMP_ULE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  MASK_VECTOR_NANS(Ty, Src1, Src2, true)
  IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OLE)
  return executeFCMP_OLE(Src1, Src2, Ty);    
}

GenericValue InstrsInterpreter::executeFCMP_UGE(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  MASK_VECTOR_NANS(Ty, Src1, Src2, true)
  IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OGE)
  return executeFCMP_OGE(Src1, Src2, Ty);    
}

GenericValue InstrsInterpreter::executeFCMP_ULT(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  MASK_VECTOR_NANS(Ty, Src1, Src2, true)
  IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OLT)
  return executeFCMP_OLT(Src1, Src2, Ty);   
}

GenericValue InstrsInterpreter::executeFCMP_UGT(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  MASK_VECTOR_NANS(Ty, Src1, Src2, true)
  IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OGT)
  return executeFCMP_OGT(Src1, Src2, Ty);    
}

GenericValue InstrsInterpreter::executeFCMP_ORD(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  if(Ty->isVectorTy()) {
    assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());
    Dest.AggregateVal.resize( Src1.AggregateVal.size() );
    if (cast<VectorType>(Ty)->getElementType()->isFloatTy()) {
      for( size_t _i=0;_i<Src1.AggregateVal.size();_i++)
        Dest.AggregateVal[_i].IntVal = APInt(1,
        ( (Src1.AggregateVal[_i].FloatVal ==
        Src1.AggregateVal[_i].FloatVal) &&
        (Src2.AggregateVal[_i].FloatVal ==
        Src2.AggregateVal[_i].FloatVal)));
    } else {
      for( size_t _i=0;_i<Src1.AggregateVal.size();_i++)
        Dest.AggregateVal[_i].IntVal = APInt(1,
        ( (Src1.AggregateVal[_i].DoubleVal ==
        Src1.AggregateVal[_i].DoubleVal) &&
        (Src2.AggregateVal[_i].DoubleVal ==
        Src2.AggregateVal[_i].DoubleVal)));
    }
  } else if (Ty->isFloatTy())
    Dest.IntVal = APInt(1,(Src1.FloatVal == Src1.FloatVal &&
                           Src2.FloatVal == Src2.FloatVal));
  else {
    Dest.IntVal = APInt(1,(Src1.DoubleVal == Src1.DoubleVal &&
                           Src2.DoubleVal == Src2.DoubleVal));
  }
  return Dest;
}

GenericValue InstrsInterpreter::executeFCMP_UNO(GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Dest;
  if(Ty->isVectorTy()) {
    assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());
    Dest.AggregateVal.resize( Src1.AggregateVal.size() );
    if (cast<VectorType>(Ty)->getElementType()->isFloatTy()) {
      for( size_t _i=0;_i<Src1.AggregateVal.size();_i++)
        Dest.AggregateVal[_i].IntVal = APInt(1,
        ( (Src1.AggregateVal[_i].FloatVal !=
           Src1.AggregateVal[_i].FloatVal) ||
          (Src2.AggregateVal[_i].FloatVal !=
           Src2.AggregateVal[_i].FloatVal)));
      } else {
        for( size_t _i=0;_i<Src1.AggregateVal.size();_i++)
          Dest.AggregateVal[_i].IntVal = APInt(1,
          ( (Src1.AggregateVal[_i].DoubleVal !=
             Src1.AggregateVal[_i].DoubleVal) ||
            (Src2.AggregateVal[_i].DoubleVal !=
             Src2.AggregateVal[_i].DoubleVal)));
      }
  } else if (Ty->isFloatTy())
    Dest.IntVal = APInt(1,(Src1.FloatVal != Src1.FloatVal ||
                           Src2.FloatVal != Src2.FloatVal));
  else {
    Dest.IntVal = APInt(1,(Src1.DoubleVal != Src1.DoubleVal ||
                           Src2.DoubleVal != Src2.DoubleVal));
  }
  return Dest;    
}

GenericValue InstrsInterpreter::executeFCMP_BOOL(GenericValue Src1, GenericValue Src2, Type *Ty, bool val) {
  GenericValue Dest;
    if(Ty->isVectorTy()) {
      assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());
      Dest.AggregateVal.resize( Src1.AggregateVal.size() );
      for( size_t _i=0; _i<Src1.AggregateVal.size(); _i++)
        Dest.AggregateVal[_i].IntVal = APInt(1,val);
    } else {
      Dest.IntVal = APInt(1, val);
    }

    return Dest;    
}

GenericValue InstrsInterpreter::executeCmpInst(unsigned predicate, GenericValue Src1, GenericValue Src2, Type *Ty) {
  GenericValue Result;
  switch (predicate) {
  case ICmpInst::ICMP_EQ:    return executeICMP_EQ(Src1, Src2, Ty);
  case ICmpInst::ICMP_NE:    return executeICMP_NE(Src1, Src2, Ty);
  case ICmpInst::ICMP_UGT:   return executeICMP_UGT(Src1, Src2, Ty);
  case ICmpInst::ICMP_SGT:   return executeICMP_SGT(Src1, Src2, Ty);
  case ICmpInst::ICMP_ULT:   return executeICMP_ULT(Src1, Src2, Ty);
  case ICmpInst::ICMP_SLT:   return executeICMP_SLT(Src1, Src2, Ty);
  case ICmpInst::ICMP_UGE:   return executeICMP_UGE(Src1, Src2, Ty);
  case ICmpInst::ICMP_SGE:   return executeICMP_SGE(Src1, Src2, Ty);
  case ICmpInst::ICMP_ULE:   return executeICMP_ULE(Src1, Src2, Ty);
  case ICmpInst::ICMP_SLE:   return executeICMP_SLE(Src1, Src2, Ty);
  case FCmpInst::FCMP_ORD:   return executeFCMP_ORD(Src1, Src2, Ty);
  case FCmpInst::FCMP_UNO:   return executeFCMP_UNO(Src1, Src2, Ty);
  case FCmpInst::FCMP_OEQ:   return executeFCMP_OEQ(Src1, Src2, Ty);
  case FCmpInst::FCMP_UEQ:   return executeFCMP_UEQ(Src1, Src2, Ty);
  case FCmpInst::FCMP_ONE:   return executeFCMP_ONE(Src1, Src2, Ty);
  case FCmpInst::FCMP_UNE:   return executeFCMP_UNE(Src1, Src2, Ty);
  case FCmpInst::FCMP_OLT:   return executeFCMP_OLT(Src1, Src2, Ty);
  case FCmpInst::FCMP_ULT:   return executeFCMP_ULT(Src1, Src2, Ty);
  case FCmpInst::FCMP_OGT:   return executeFCMP_OGT(Src1, Src2, Ty);
  case FCmpInst::FCMP_UGT:   return executeFCMP_UGT(Src1, Src2, Ty);
  case FCmpInst::FCMP_OLE:   return executeFCMP_OLE(Src1, Src2, Ty);
  case FCmpInst::FCMP_ULE:   return executeFCMP_ULE(Src1, Src2, Ty);
  case FCmpInst::FCMP_OGE:   return executeFCMP_OGE(Src1, Src2, Ty);
  case FCmpInst::FCMP_UGE:   return executeFCMP_UGE(Src1, Src2, Ty);
  case FCmpInst::FCMP_FALSE: return executeFCMP_BOOL(Src1, Src2, Ty, false);
  case FCmpInst::FCMP_TRUE:  return executeFCMP_BOOL(Src1, Src2, Ty, true);
  default:
    dbgs() << "Unhandled Cmp predicate\n";
    llvm_unreachable(nullptr);
  }    
}

GenericValue InstrsInterpreter::executeSelectInst(GenericValue Src1, GenericValue Src2, GenericValue Src3, Type *Ty) {
    GenericValue Dest;
    if(Ty->isVectorTy()) {
      assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());
      assert(Src2.AggregateVal.size() == Src3.AggregateVal.size());
      Dest.AggregateVal.resize( Src1.AggregateVal.size() );
      for (size_t i = 0; i < Src1.AggregateVal.size(); ++i)
        Dest.AggregateVal[i] = (Src1.AggregateVal[i].IntVal == 0) ?
          Src3.AggregateVal[i] : Src2.AggregateVal[i];
    } else {
      Dest = (Src1.IntVal == 0) ? Src3 : Src2;
    }
    return Dest;    
}

//===----------------------------------------------------------------------===//
//                    CFA Assignment Execution
//===----------------------------------------------------------------------===//

GenericValue InstrsInterpreter::executeCFAAssignment(CFAInstruction &I) {
  const auto &Assign = I.getAssign();
  if (!Assign) {
    llvm_unreachable("Invalid assignment instruction");
  }
  
  Value *formalArg = Assign->formalArg;
  Value *actualArg = Assign->actualArg;
  
  // Get the value of the actual argument
  GenericValue actualValue = getOperandValue(actualArg);

  // Set the formal argument to the actual value
  SetValue(formalArg, actualValue);
  // llvm::outs() << "actualValue: " << actualValue.PointerVal << "\n";
  // llvm::outs() << "formalValue: " << getOperandValue(formalArg).PointerVal << "\n";
  return actualValue;
}

//===----------------------------------------------------------------------===//
//                    Main Instruction Visit Methods
//===----------------------------------------------------------------------===//

void InstrsInterpreter::visitCFAInstruction(CFAInstruction &I) {
  if (I.getType() == CFAInstruction::EXPAND_ASSIGNMENT) {
    executeCFAAssignment(I);
  } else if (I.getType() == CFAInstruction::LLVM_IR_INSTRUCTION) {
    Instruction *Instr = I.getLLVMInstr();
    if (!Instr) llvm_unreachable("Null LLVM instruction in CFAInstruction");
    // Local dispatch on instruction type; only delegate calls to base
    switch (Instr->getOpcode()) {
    default:
      LLVM_ERROR("Unknown instruction type encountered!" << Instr);
      llvm_unreachable(nullptr);
    case Instruction::Ret:
      return CFAVisitReturnInst(static_cast<ReturnInst&>(*Instr));
    case Instruction::Br:
      return CFAVisitBranchInst(static_cast<BranchInst&>(*Instr));
    case Instruction::Switch:
      return CFAVisitSwitchInst(static_cast<SwitchInst&>(*Instr));
    case Instruction::IndirectBr:
      return CFAVisitIndirectBrInst(static_cast<IndirectBrInst&>(*Instr));
    case Instruction::PHI:
      return CFAVisitPHINode(static_cast<PHINode&>(*Instr));
    case Instruction::FNeg:
      return CFAVisitUnaryOperator(static_cast<UnaryOperator&>(*Instr));
    case Instruction::Add:
    case Instruction::FAdd:
    case Instruction::Sub:
    case Instruction::FSub:
    case Instruction::Mul:
    case Instruction::FMul:
    case Instruction::UDiv:
    case Instruction::SDiv:
    case Instruction::FDiv:
    case Instruction::URem:
    case Instruction::SRem:
    case Instruction::FRem:
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor:
      return CFAVisitBinaryOperator(static_cast<BinaryOperator&>(*Instr));
    case Instruction::Shl:
      return CFAVisitShl(static_cast<BinaryOperator&>(*Instr));
    case Instruction::LShr:
      return CFAVisitLShr(static_cast<BinaryOperator&>(*Instr));
    case Instruction::AShr:
      return CFAVisitAShr(static_cast<BinaryOperator&>(*Instr));
    case Instruction::ICmp:
      return CFAVisitICmpInst(static_cast<ICmpInst&>(*Instr));
    case Instruction::FCmp:
      return CFAVisitFCmpInst(static_cast<FCmpInst&>(*Instr));
    case Instruction::Alloca:
      return CFAVisitAllocaInst(static_cast<AllocaInst&>(*Instr));
    case Instruction::Load:
      return CFAVisitLoadInst(static_cast<LoadInst&>(*Instr));
    case Instruction::Store:
      return CFAVisitStoreInst(static_cast<StoreInst&>(*Instr));
    case Instruction::GetElementPtr:
      return CFAVisitGetElementPtrInst(static_cast<GetElementPtrInst&>(*Instr));
    case Instruction::Trunc:
      return CFAVisitTruncInst(static_cast<TruncInst&>(*Instr));
    case Instruction::ZExt:
      return CFAVisitZExtInst(static_cast<ZExtInst&>(*Instr));
    case Instruction::SExt:
      return CFAVisitSExtInst(static_cast<SExtInst&>(*Instr));
    case Instruction::FPTrunc:
      return CFAVisitFPTruncInst(static_cast<FPTruncInst&>(*Instr));
    case Instruction::FPExt:
      return CFAVisitFPExtInst(static_cast<FPExtInst&>(*Instr));
    case Instruction::UIToFP:
      return CFAVisitUIToFPInst(static_cast<UIToFPInst&>(*Instr));
    case Instruction::SIToFP:
      return CFAVisitSIToFPInst(static_cast<SIToFPInst&>(*Instr));
    case Instruction::FPToUI:
      return CFAVisitFPToUIInst(static_cast<FPToUIInst&>(*Instr));
    case Instruction::FPToSI:
      return CFAVisitFPToSIInst(static_cast<FPToSIInst&>(*Instr));
    case Instruction::PtrToInt:
      return CFAVisitPtrToIntInst(static_cast<PtrToIntInst&>(*Instr));
    case Instruction::IntToPtr:
      return CFAVisitIntToPtrInst(static_cast<IntToPtrInst&>(*Instr));
    case Instruction::BitCast:
      return CFAVisitBitCastInst(static_cast<BitCastInst&>(*Instr));
    case Instruction::Select:
      return CFAVisitSelectInst(static_cast<SelectInst&>(*Instr));
    case Instruction::VAArg:
      return CFAVisitVAArgInst(static_cast<VAArgInst&>(*Instr));
    case Instruction::ExtractElement:
      return CFAVisitExtractElementInst(static_cast<ExtractElementInst&>(*Instr));
    case Instruction::InsertElement:
      return CFAVisitInsertElementInst(static_cast<InsertElementInst&>(*Instr));
    case Instruction::ShuffleVector:
      return CFAVisitShuffleVectorInst(static_cast<ShuffleVectorInst&>(*Instr));
    case Instruction::ExtractValue:
      return CFAVisitExtractValueInst(static_cast<ExtractValueInst&>(*Instr));
    case Instruction::InsertValue:
      return CFAVisitInsertValueInst(static_cast<InsertValueInst&>(*Instr));
    case Instruction::Call:
    case Instruction::Invoke:
    case Instruction::CallBr: {
      // Check if this is an intrinsic call
      if (auto *II = dyn_cast<IntrinsicInst>(Instr)) {
        // Handle specific VA intrinsics
        switch (II->getIntrinsicID()) {
        case Intrinsic::vastart:
          return CFAVisitVAStartInst(static_cast<VAStartInst&>(*II));
        case Intrinsic::vaend:
          return CFAVisitVAEndInst(static_cast<VAEndInst&>(*II));
        case Intrinsic::vacopy:
          return CFAVisitVACopyInst(static_cast<VACopyInst&>(*II));
        default:
          return CFAVisitIntrinsicInst(*II);
        }
      }
      // Other calls handled by CFAVisitCallBase
      return CFAVisitCallBase(static_cast<CallBase&>(*Instr));
    }
    case Instruction::Unreachable:
      return CFAVisitUnreachableInst(static_cast<UnreachableInst&>(*Instr));
    }
    
    // Not supported in CFA local exec
    llvm_unreachable("Instruction not supported by CFA execution");
  } else {
    llvm_unreachable("Unknown CFAInstruction type");
  }
}

void InstrsInterpreter::runInstrs(std::vector<CFAInstruction> &Instrs) {
  for (CFAInstruction &I : Instrs) {
    visitCFAInstruction(I);
  }
}
