#include "structExpander.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"

PreservedAnalyses StructExpander::run(Module &M, ModuleAnalysisManager &MAM) {
    // Iterate through target struct names
    for (const auto &structName : targetStructNames) {
        // Find the struct type by name
        StructType *ST = findStructByName(M, structName);
        
        if (!ST) {
            ERROR("Struct type not found: " << structName);
            continue;
        }
        
        // Create output file for this struct
        std::ofstream csvFile(csvFilePath + structName + ".csv");
        if (!csvFile.is_open()) {
            ERROR("Failed to open output file " + structName + ".csv");
            continue;
        }
        
        // Expand the struct
        expandStruct(M, ST, csvFile, structName);
        csvFile.close();
    }
    
    return PreservedAnalyses::all();
}

StructType* StructExpander::findStructByName(Module &M, const std::string &structName) {
    // Search for struct type by exact name match (absolute name)
    for (StructType *ST : M.getIdentifiedStructTypes()) {
        std::string typeName = ST->getName().str();
        
        // Use exact/absolute name matching
        if (typeName == structName) {
            return ST;
        }
    }
    
    return nullptr;
}

void StructExpander::expandStruct(Module &M, StructType *ST, std::ofstream &csvFile, const std::string &structName) {
    if (!ST) {
        ERROR("StructType is null.");
        return;
    }
    
    // Clear visited types for a new expansion
    visitedTypes.clear();
    
    // Start expansion from offset 0
    expandStructMembers(M, ST, csvFile, structName, 0);
}

std::string StructExpander::getTypeString(Type *type) {
    std::string typeStr;
    llvm::raw_string_ostream rso(typeStr);
    
    if (!type) {
        return "unknown";
    }
    
    // Handle pointer types
    if (PointerType *ptrType = dyn_cast<PointerType>(type)) {
        return "ptr";
    }
    
    // Handle integer types
    if (IntegerType *intType = dyn_cast<IntegerType>(type)) {
        unsigned bitWidth = intType->getBitWidth();
        if (bitWidth == 1) {
            return "i1";
        } else if (bitWidth == 8) {
            return "i8";
        } else if (bitWidth == 16) {
            return "i16";
        } else if (bitWidth == 32) {
            return "i32";
        } else if (bitWidth == 64) {
            return "i64";
        } else {
            return "i" + std::to_string(bitWidth);
        }
    }
    
    // Handle floating point types
    if (type->isFloatTy()) {
        return "float";
    }
    if (type->isDoubleTy()) {
        return "double";
    }
    
    // Handle struct types
    if (StructType *structType = dyn_cast<StructType>(type)) {
        if (structType->hasName()) {
            return structType->getName().str();
        } else {
            return "anon_struct";
        }
    }
    
    // Handle array types
    if (ArrayType *arrayType = dyn_cast<ArrayType>(type)) {
        Type *elemType = arrayType->getElementType();
        uint64_t numElements = arrayType->getNumElements();
        return "array_" + std::to_string(numElements) + "_" + getTypeString(elemType);
    }
    
    // Handle vector types
    if (type->isVectorTy()) {
        return "vector";
    }
    
    // Default fallback
    type->print(rso);
    return rso.str();
}

void StructExpander::expandStructMembers(Module &M, StructType *ST, std::ofstream &csvFile, 
                                        const std::string &currentPath, uint64_t baseOffsetBytes) {
    if (!ST) {
        return;
    }
    
    // Check if we've already visited this type to avoid infinite recursion
    if (visitedTypes.find(ST) != visitedTypes.end()) {
        return;
    }
    visitedTypes.insert(ST);
    
    // Get the data layout for offset calculations
    const DataLayout &DL = M.getDataLayout();
    const StructLayout *SL = DL.getStructLayout(ST);
    
    // Iterate through all elements of the struct
    unsigned numElements = ST->getNumElements();
    for (unsigned i = 0; i < numElements; ++i) {
        Type *elementType = ST->getElementType(i);
        uint64_t elementOffset = SL->getElementOffset(i);
        
        // Get type string for this element
        std::string typeString = getTypeString(elementType);
        
        // Generate member path using hyphen separator with type
        std::string memberPath = currentPath + "-" + typeString;
        uint64_t totalOffset = baseOffsetBytes + elementOffset;
        
        // Write this member to CSV
        csvFile << memberPath << "," << totalOffset << "\n";
        
        // If the element is a struct type, recursively expand it
        if (StructType *nestedST = dyn_cast<StructType>(elementType)) {
            // Temporarily remove from visited set to allow expansion
            visitedTypes.erase(ST);
            expandStructMembers(M, nestedST, csvFile, memberPath, totalOffset);
            visitedTypes.insert(ST);
        }
        // If it's an array type, we can expand it too
        else if (ArrayType *arrayType = dyn_cast<ArrayType>(elementType)) {
            Type *arrayElementType = arrayType->getElementType();
            uint64_t arrayElementSize = DL.getTypeAllocSize(arrayElementType);
            uint64_t numArrayElements = arrayType->getNumElements();
            
            // Expand array elements
            for (uint64_t j = 0; j < numArrayElements; ++j) {
                std::string arrayElemTypeStr = getTypeString(arrayElementType);
                std::string arrayElementPath = memberPath + "-" + std::to_string(j) + "-" + arrayElemTypeStr;
                uint64_t arrayElementOffset = totalOffset + (j * arrayElementSize);
                
                csvFile << arrayElementPath << "," << arrayElementOffset << "\n";
                
                // If array element is a struct, expand it
                if (StructType *arrayNestedST = dyn_cast<StructType>(arrayElementType)) {
                    visitedTypes.erase(ST);
                    expandStructMembers(M, arrayNestedST, csvFile, arrayElementPath, arrayElementOffset);
                    visitedTypes.insert(ST);
                }
            }
        }
    }
    
    // Remove from visited set when done with this level
    visitedTypes.erase(ST);
}

void runStructExpander(Module &M, std::set<std::string> targetStructNames, std::string output_dir) {
    llvm::PassBuilder PB;
    llvm::ModulePassManager MPM;
    llvm::ModuleAnalysisManager MAM;

    // Register analysis passes with the managers
    PB.registerModuleAnalyses(MAM);
    StructExpander structExpander(targetStructNames, output_dir);
    MPM.addPass(std::move(structExpander));
    MPM.run(M, MAM);
}

int main(int argc, char **argv) {
    cl::opt<std::string> IRPath(cl::Positional, cl::desc("<IR path>"), cl::Required);
    cl::list<std::string> structNames("structNames", cl::desc("Input struct names"), cl::CommaSeparated);
    cl::opt<std::string> output_dir("output-dir", cl::desc("Output directory"), cl::Required);

    cl::ParseCommandLineOptions(argc, argv, "Struct Expander - Expand struct types without debug info");

    std::set<std::string> targetStructNames(structNames.begin(), structNames.end());

    if (targetStructNames.empty()) {
        ERROR("No struct names provided. Use -structNames=StructName1,StructName2");
        return 1;
    }

    LLVMContext Context;
    SMDiagnostic Err;
    std::unique_ptr<Module> M = parseIRFile(IRPath, Err, Context);
    if (!M) {
        Err.print(argv[0], errs());
        return 1;
    }

    runStructExpander(*M, targetStructNames, output_dir);

    return 0;
}
