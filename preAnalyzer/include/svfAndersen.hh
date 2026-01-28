#ifndef SVFANDERSEN_H
#define SVFANDERSEN_H
#include <llvm/IR/LLVMContext.h>
#include "SVF-LLVM/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "WPA/Andersen.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "MemoryModel/PointerAnalysis.h"

namespace svfAndersen{
    void runSVFAndersen(llvm::Module& M);
}



#endif // SVFANDERSEN_H