/*
    kleeEntryBuilder Pass

    1. Create a global byte array variable for all heap variables: uint8_t heap_bytes[sizeof( heap size )];
    2. Symbolize all global variables and heap_bytes by calling klee_make_symbolic respectively;
    3. Concretize pointers based on the parsing results of pb.
*/

#ifndef KLEE_KLEE_ENTRY_BUILDER_HH
#define KLEE_KLEE_ENTRY_BUILDER_HH

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>
#include "llvm/Passes/PassBuilder.h"

#include<vector>
#include<map>
#include<string>
#include<cassert>
#include <iostream>
#include <fstream>

#include "macro.hh"

#include "modelInputs.pb.h"
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace KLEE{

struct GVAddrInfo{
    llvm::GlobalVariable* gv;
    uint32_t offset;

    GVAddrInfo(llvm::GlobalVariable* gv, uint32_t offset) : gv(gv), offset(offset) {}
};

struct Region {
    std::string name;
    uint32_t start;
    uint32_t end;

    Region() = default;
    Region(std::string n, uint32_t s, uint32_t e) : name(std::move(n)), start(s), end(e) {}
};

// When representing pointer relationships, src represents the position of the source pointer member in its global object, dest represents the position of the target address in its global object
// When representing address ranges, src represents the starting address in its global object, dest represents the ending address in its global object
struct MemLink{
    GVAddrInfo src;
    GVAddrInfo dest;

    MemLink(GVAddrInfo src, GVAddrInfo dest) : src(src), dest(dest) {}
};

void removeTypeTestAndAssume(llvm::Module &M);
void simplifyComplexFunctions(llvm::Module &M);
void runKleeEntryBuilder(llvm::Module &M, std::string pbPath, std::string entryF, std::map<int, std::pair<std::string, std::string>> paramList, std::string elfFile, std::string currentProperty = "");

class kleeEntryBuilder : public llvm::PassInfoMixin<kleeEntryBuilder>{
public:
    // constructor
    kleeEntryBuilder(std::string entryF, std::string pbPath, std::map<int, std::pair<std::string, std::string>> paramList, std::string elfFile, std::string currentProperty = "") 
        : _entryF(entryF), _paramList(paramList), _elfFile(elfFile), _currentProperty(currentProperty) {
        std::ifstream input(pbPath);
        if (!input.is_open()) {
            ERROR("Failed to open file " + pbPath);
            return;
        }
        google::protobuf::io::IstreamInputStream zero_copy_stream(&input);
        google::protobuf::io::CodedInputStream coded_stream(&zero_copy_stream);
        _modelInputs.ParseFromCodedStream(&coded_stream);
        input.close();
    }

    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM){
        createAPIFunctionDeclares(M);
        if (!_elfFile.empty()){
            parseELFFile(M);
        }
        createHeapBytes(M);
        createAppendToBytesFunctions(M);
        collectAllGlobals(M);
        loadParamsConfig(M);
        createKleeEntry(M);
        return llvm::PreservedAnalyses::all();
    }

    static llvm::StringRef name() { return "kleeEntryBuilder"; }

    static bool isRequired() { return true; }

private:
    void createAPIFunctionDeclares(llvm::Module &M);

    void createHeapBytes(llvm::Module &M);

    void createAppendToBytesFunctions(llvm::Module &M);

    void collectAllGlobals(llvm::Module &M);

    std::pair<llvm::GlobalVariable*, uint32_t> handlePointToAddr(std::string pointTo);

    void concretizePointer(llvm::IRBuilder<>& builder, llvm::Module& M, MemLink& link);

    void symbolicBasicTyGVs(llvm::IRBuilder<>& builder, llvm::Module& M);

    void symbolicBasicTyHeapVars(llvm::IRBuilder<>& builder, llvm::Module& M);

    void symbolicAddrRange(llvm::IRBuilder<>& builder, llvm::Module& M, MemLink& link);

    void bindPtrRelations(llvm::IRBuilder<>& builder, llvm::Module& M);

    void symbolicAddrRanges(llvm::IRBuilder<>& builder, llvm::Module& M);

    // Map ptr value to region name + offset
    std::map<uint64_t, std::string> processPtrValues();

    void concretizeHeap(llvm::IRBuilder<>& builder, llvm::Module& M);

    void callEntryFunction(llvm::IRBuilder<>& builder, llvm::Module& M);

    void createKleeEntry(llvm::Module &M);
    
    // Concretize all global variables and heap base parsed data
    void appendValueToBytes(llvm::Value* target, uint32_t size, uint64_t offset, modelInputs::TypeSpec type, std::string valueStr, llvm::IRBuilder<>& builder, llvm::Module& M);
    void concretizeGlobalsAndHeap(llvm::IRBuilder<>& builder, llvm::Module &M);

    // Analyze global variables, divide them into basic type global variables, pointer relationships, and address ranges
    void analysisGVs(llvm::Module &M);

    // Parse ELF file to get all constant global variables and their address ranges
    void parseELFFile(llvm::Module &M);

    // Load params_config.yml and create global variables for parameter configurations
    void loadParamsConfig(llvm::Module &M);

    // Store all basic type global variables
    std::vector<llvm::GlobalVariable*> _basicTyGVs;
    // Store all basic type heap variables, represented by their offsets in heap_bytes
    std::vector<std::pair<uint64_t, uint64_t>> _basicTyHeapVars;
    // Store all pointer relationships
    std::vector<MemLink> _ptrRelations;
    // Store all address ranges
    // std::vector<MemLink> _addrRanges;
    std::vector<MemLink> _addrRangesNeedSymbolized;
    // Store all global variables
    std::map<std::string, llvm::GlobalVariable*> _allGlobals;
    // Store all constants
    std::map<std::string, llvm::GlobalVariable*> _constGlobals;
    // Store all constant global variables and its address ranges.
    // This is parse from the elf file related to the target property.
    // For string, we use the longest in IR to represent the whole range of strings section.
    std::map<llvm::GlobalVariable*, std::pair<uint32_t, uint32_t>> _constGVAddrRanges;
    llvm::GlobalVariable* _longestStrConst = nullptr; // The longest string constant in IR
    llvm::GlobalVariable* _heapBytes; // uint8_t heap[sizeof(heap)]
    uint32_t _gvsize = 0; // The size of all global variables
    uint32_t _hsize = 0; // The size of the heap

    llvm::Function* _memcpy = nullptr;
    llvm::Function* _kleeMakeSymbolic = nullptr;

    llvm::Function* _append_i8_to_bytes = nullptr;
    llvm::Function* _append_i16_to_bytes = nullptr;
    llvm::Function* _append_i32_to_bytes = nullptr;
    llvm::Function* _append_i64_to_bytes = nullptr;
    llvm::Function* _append_float_to_bytes = nullptr;
    llvm::Function* _append_double_to_bytes = nullptr;

    std::string _entryF;
    modelInputs::ModelInputs _modelInputs;
    std::map<int, std::pair<std::string, std::string>> _paramList;
    std::string _elfFile;
    std::string _currentProperty;
    bool _isArducopter = false;
    bool _isPX4 = false;
    
    // Storage for params_config data: maps param_id to (GlobalVariable, data_size)
    std::map<int, std::pair<llvm::GlobalVariable*, size_t>> _paramsConfigGlobals;

    struct skippedHeapRange{
        size_t hash;
        uint32_t startIndex;
        uint32_t endIndex;

        skippedHeapRange(size_t h, uint32_t s, uint32_t e) : hash(h), startIndex(s), endIndex(e) {}
    };

    const std::vector<skippedHeapRange> _skippedHeapRanges = {
        skippedHeapRange(7101999869335443201, 1200, 1480),
        skippedHeapRange(15114867476137835068, 9240, 9464)
        // skippedHeapRange(7101999869335443201, 1472, 1480)
    };

    // Map from property name to a set of hash values that should be symbolized for that property
    // The entire range of each hash will be automatically symbolized
    const std::map<std::string, std::vector<size_t>> _propertySymbolizedHashes = {
        {"A_ALT_HOLD1", {15114867476137835068ULL}},
        {"A_GPS_FS2", {15114867476137835068ULL}}
    };

    struct skippedGVRange{
        std::string gvName;
        uint32_t startIndex;
        uint32_t endIndex;

        skippedGVRange(std::string g, uint32_t s, uint32_t e) : gvName(g), startIndex(s), endIndex(e) {}
    };

    const std::vector<skippedGVRange> _skippedGVRanges = {

    };
};

}
#endif // KLEE_KLEE_ENTRY_BUILDER_HH
