//===- GenericValue.h - Represent any type of LLVM value --------*- C++ -*-===//
//
// The GenericValue class is used to represent an LLVM value of arbitrary type.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_GENERICVALUE_H
#define LLVM_EXECUTIONENGINE_GENERICVALUE_H

#include "llvm/ADT/APInt.h"
#include <vector>

using namespace llvm;

namespace uppllvm {

using PointerTy = void *;

struct GenericValue {
  struct IntPair {
    unsigned int first;
    unsigned int second;
  };
  union {
    double DoubleVal;
    float FloatVal;
    PointerTy PointerVal;
    struct IntPair UIntPairVal; // Used for i64 on 32-bit hosts.
                                    // Stores 64-bit integers as two 32-bit parts:
                                    // first = lower 32 bits, second = upper 32 bits
                                    // Example: 0x123456789ABCDEF0 -> first=0x9ABCDEF0, second=0x12345678
    unsigned char Untyped[8];
  };
  APInt IntVal; //  Represents integers of any bit width, also used for long doubles.
  // For aggregate data types.
  std::vector<GenericValue> AggregateVal;

  // to make code faster, set GenericValue to zero could be omitted, but it is
  // potentially can cause problems, since GenericValue to store garbage
  // instead of zero.
  GenericValue() : IntVal(1, 0) {
    UIntPairVal.first = 0;
    UIntPairVal.second = 0;
  }
  explicit GenericValue(void *V) : PointerVal(V), IntVal(1, 0) {}
};

inline GenericValue PTOGV(void *P) { return GenericValue(P); }
inline void *GVTOP(const GenericValue &GV) { return GV.PointerVal; }

} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_GENERICVALUE_H
