#include "kleeEntryBuilder.hh"
#include "config_parse.hpp"
#include "user_utils.hh"
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/WithColor.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/CommandLine.h>
#include <chrono> 

void dumpModule(llvm::Module& M, std::string IR_path){
    size_t pos = IR_path.rfind(".ll");
    if (pos != std::string::npos) {
        IR_path = IR_path.substr(0, pos);
    }
    std::string OutputFileName = IR_path + "_klee.ll";

    std::error_code EC;
    size_t dirPos = OutputFileName.rfind('/');
    if (dirPos != std::string::npos) {
        std::string dirPath = OutputFileName.substr(0, dirPos);
        EC = llvm::sys::fs::create_directories(dirPath);
        if (EC) {
            LLVM_ERROR("Could not create directories: " << EC.message() << "\n");
            abort();
        }
    }

    llvm::raw_fd_ostream OutFile(OutputFileName, EC, llvm::sys::fs::OF_None);
    if (EC) {
        LLVM_ERROR("Could not open file: " << EC.message() << "\n");
        abort();
    }   
    M.print(OutFile, nullptr);
}

void parseCommandLineOptions(int argc, char **argv, std::string& seed, std::string& elf) {
    llvm::cl::opt<std::string> pbPath("seed", 
        llvm::cl::desc("Specify the input seed file"),
        llvm::cl::value_desc("seed")
    );

    llvm::cl::opt<std::string> elfFile("elf", 
        llvm::cl::desc("Specify the target elf file"),
        llvm::cl::value_desc("elf")
    );
    llvm::cl::ParseCommandLineOptions(argc, argv);

    if (pbPath.empty()) {
        LLVM_ERROR("Please specify the input seed binary file");
        abort();
    }
    seed = pbPath;
    elf = elfFile;
}

int main(int argc, char** argv) {
    auto start = std::chrono::high_resolution_clock::now();
    std::string seed = "";
    std::string elf = "";
    parseCommandLineOptions(argc, argv, seed, elf);

    std::string dtmc = utils::getEnv("DTMC");

    if (dtmc == "") {
        ERROR("Please set DTMC environment variable by export DTMC=/path/to/modelCheckingFlightControl");
        std::abort();
    }

    std::string IR_config_path = dtmc + "/configs/IR_config.yml";
    IRConfig config = parseConfig(IR_config_path);
    std::string IR_path = dtmc + "/" + config.base.ir;
    std::string curr_property = config.base.property_name;
    std::string IR_OutputPath = utils::getDirectoryPath(IR_path) + "/" + curr_property + "/" + utils::getFileName(IR_path);
    IR_path = IR_OutputPath;

    llvm::LLVMContext Context;
    llvm::SMDiagnostic Err;
    std::unique_ptr<llvm::Module> M = parseIRFile(IR_path, Err, Context);

    if (!M) {
        llvm::WithColor::error(llvm::errs(), argv[0]) << "Error reading IR file: " << IR_path << "\n";
        Err.print(argv[0], llvm::errs());
        return 1;
    } 

    KLEE::runKleeEntryBuilder(*M, seed, config.base.entrypoints[0], config.paramList, elf, curr_property);

    dumpModule(*M, IR_OutputPath);
    INFO("KLEE helper pass finished!");
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    INFO("Total time:" << duration.count() << " ms");

    return 0;
}