#ifndef INCALL_H
#define INCALL_H
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include <map>
#include <set>

namespace incall{
    struct IndirectCallEdge{
        llvm::CallBase &incallsite;
        bool isVirtual = false;
        std::set<llvm::Function*> candidates;
        
        IndirectCallEdge(llvm::CallBase &site, const std::set<llvm::Function*>& funcs)
        : incallsite(site), candidates(funcs) {}

        IndirectCallEdge(llvm::CallBase &site, bool is_v, const std::set<llvm::Function*>& funcs)
        : incallsite(site), isVirtual(is_v), candidates(funcs) {}
    };

    void dumpIndirectCalls(std::string output, std::map<int, incall::IndirectCallEdge> &incallEdges);
}

extern std::map<int, incall::IndirectCallEdge> TypeMDIncallEdges;
extern std::map<int, incall::IndirectCallEdge> SVFIncallEdges;
extern std::map<int, incall::IndirectCallEdge> DeepTypeIncallEdges;

#endif