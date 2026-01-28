#ifndef STRUCT_EXPANDER_H
#define STRUCT_EXPANDER_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>

#include "macro.hh"

using namespace llvm;

class StructExpander : public PassInfoMixin<StructExpander> {
public:
    StructExpander(std::set<std::string> targetStructNames, std::string csvFilePath) 
        : targetStructNames(targetStructNames), csvFilePath(csvFilePath) {};
    
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);

private:
    void expandStruct(Module &M, StructType *ST, std::ofstream &csvFile, const std::string &structName);
    void expandStructMembers(Module &M, StructType *ST, std::ofstream &csvFile, 
                           const std::string &currentPath, uint64_t baseOffsetBytes);
    StructType* findStructByName(Module &M, const std::string &structName);
    std::string getTypeString(Type *type);
    
    std::set<std::string> targetStructNames;
    std::string csvFilePath;
    std::set<Type*> visitedTypes;  // To avoid infinite recursion
};

void runStructExpander(Module &M, std::set<std::string> targetStructNames, std::string output_dir = "./");

#endif // STRUCT_EXPANDER_H
