#include "modelInputTempGen.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"

#include "user_utils.hh"

using namespace llvm;

int main(int argc, char **argv) {
    cl::opt<std::string> IRPath(cl::Positional, cl::desc("<input IR file>"), cl::Required);
    cl::opt<std::string> outputDir("output", cl::desc("Output directory"), cl::value_desc("directory"));
    cl::alias outputDirAlias("o", cl::desc("Alias for --output"), cl::aliasopt(outputDir));

    cl::ParseCommandLineOptions(argc, argv, "Model Input Template Generator - A tool for generating model input templates");


    LLVMContext Context;
    SMDiagnostic Err;
    std::unique_ptr<Module> M = parseIRFile(IRPath, Err, Context);
    if (!M) {
        Err.print(argv[0], errs());
        return 1;
    }
    
    std::string fc_type = "";
    if(IRPath.find("arducopter_sitl_pi64") != std::string::npos) {
        fc_type = "arducopter";
    } else if(IRPath.find("px4_hitl_pi64") != std::string::npos) {
        fc_type = "px4";
    } else {
        ERROR("Unsupported flight control type. Please provide IR file for either 'arducopter' or 'px4'.");
        return 1;
    }

    runMITGen(*M, outputDir, fc_type);
    // runSTGen(*M, outputDir);
    
    return 0;
}