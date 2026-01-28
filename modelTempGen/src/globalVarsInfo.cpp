#include "globalVarsInfo.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"

PreservedAnalyses GlobalVarsInfo::run(Module &M, ModuleAnalysisManager &MAM) {
    for (GlobalVariable &GV : M.globals()) {
        if (GV.isConstant() || GV.getName().startswith("llvm.") || GV.isDeclaration()) {
            continue;
        }

        if (targetGlobalVals.find(GV.getName().str()) != targetGlobalVals.end()) {
            std::ofstream csvFile(csvFilePath + GV.getName().str() + ".csv");
            if (!csvFile.is_open()) {
                ERROR("Failed to open output file " + GV.getName().str() + ".csv");
                return PreservedAnalyses::none();
            }
            dumpGlobalVarInfo(M, &GV, csvFile);
        }
    }
    return PreservedAnalyses::all();
}

void GlobalVarsInfo::dumpGlobalVarInfo(Module &M, GlobalVariable *GV, std::ofstream &csvFile) {
    llvm::SmallVector<llvm::DIGlobalVariableExpression *, 2> GVs;
    GV->getDebugInfo(GVs);

    if (!GVs.empty()) {
        for (auto *DGVExpr : GVs) {
            if (auto *DGV = DGVExpr->getVariable()) {
                std::string globalVarName = DGV->getName().str();
                if (globalVarName.empty()) {
                    globalVarName = DGV->getType()->getName().str();  // if name is empty, use type name
                    if (globalVarName.empty()) {
                        globalVarName = "UnnamedVariable";  // if type name is empty, use UnnamedVariable
                    }
                }

                if (llvm::DIType *DIType = DGV->getType()) {
                    printTypeMembers(DIType, M, csvFile, globalVarName, 0);
                }
            }
        }
    }

}

llvm::DICompositeType* GlobalVarsInfo::findCompleteDefinition(llvm::Module &M, llvm::DICompositeType *FwdDeclDIType) {
    if (!FwdDeclDIType) return nullptr;

    std::string typeName = FwdDeclDIType->getName().str();
    unsigned expectedTag = FwdDeclDIType->getTag();

    llvm::DebugInfoFinder Finder;

    Finder.processModule(M);
    for (auto *DType : Finder.types()) {
        if (auto *DIComposite = llvm::dyn_cast<llvm::DICompositeType>(DType)) {
            // Check for name and tag match, and ensure it's not a forward declaration
            if (DIComposite->getName().str() == typeName && 
                DIComposite->getTag() == expectedTag && 
                !(DIComposite->getFlags() & llvm::DINode::FlagFwdDecl)) {
                return DIComposite; // Found complete definition
            }
        }
    }
    return nullptr;
}

void GlobalVarsInfo::printTypeMembers(llvm::DIType *DIType, llvm::Module &M, std::ofstream &csvFile, std::string currentPath, unsigned BaseOffsetBytes) {    
    if (!DIType) {
        ERROR(DIType->getName().str() << "DIType is null.");
        return;
    }

    // Check for forward declaration and find complete definition if necessary
    if (auto *CompositeType = llvm::dyn_cast<llvm::DICompositeType>(DIType)) {
        if (CompositeType->getFlags() & llvm::DINode::FlagFwdDecl) {
            DIType = findCompleteDefinition(M, CompositeType);
            if (!DIType) {
                csvFile << "No complete definition found for," << CompositeType->getName().str() << "\n";
                return;
            }
        }
    }

    if (auto *CompositeType = llvm::dyn_cast<llvm::DICompositeType>(DIType)) {
        for (auto *Element : CompositeType->getElements()) {
            if (auto *Member = llvm::dyn_cast<llvm::DIDerivedType>(Element)) {
                // If member name is empty, use the actual name of the type (base type's name) as a substitute
                std::string memberName = Member->getName().str();
                if (memberName.empty()) {
                    if (auto *BaseType = Member->getBaseType()) {
                        memberName = BaseType->getName().str();  // Use base type's name
                    } else {
                        memberName = "UnnamedMember";  // Fallback name
                    }
                }

                // Calculate member's byte offset
                uint64_t MemberOffsetBytes = Member->getOffsetInBits() / 8;
                std::string memberPath = currentPath + "." + memberName;

                // Write CSV row
                csvFile << memberPath << "," << (BaseOffsetBytes + MemberOffsetBytes) << "\n";

                // Recursively print member's nested structure
                printTypeMembers(Member->getBaseType(), M, csvFile, memberPath, BaseOffsetBytes + MemberOffsetBytes);
            }
        }
    }
}

void runglobalVarsInfo(Module &M, std::set<std::string> targetGlobalVals, std::string output_dir) {
    llvm::PassBuilder PB;
    llvm::ModulePassManager MPM;
    llvm::ModuleAnalysisManager MAM;

    // Register analysis passes with the managers
    PB.registerModuleAnalyses(MAM);
    GlobalVarsInfo globalVarsInfo(targetGlobalVals, output_dir);
    MPM.addPass(std::move(globalVarsInfo));
    MPM.run(M, MAM);
}

int main(int argc, char **argv) {
    cl::opt<std::string> IRPath(cl::Positional, cl::desc("<IR path>"), cl::Required);
    cl::list<std::string> globalVars("globalVars", cl::desc("Input globalVars"), cl::CommaSeparated);
    cl::opt<std::string> output_dir("output-dir", cl::desc("Output directory"), cl::Required);

    cl::ParseCommandLineOptions(argc, argv, "IR and output directory");

    std::set<std::string> targetGlobalVals(globalVars.begin(), globalVars.end());

    LLVMContext Context;
    SMDiagnostic Err;
    std::unique_ptr<Module> M = parseIRFile(IRPath, Err, Context);
    if (!M) {
        Err.print(argv[0], errs());
        return 1;
    }

    runglobalVarsInfo(*M, targetGlobalVals, output_dir);

    return 0;
}