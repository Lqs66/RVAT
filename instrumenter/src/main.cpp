#include "instrumenter.hh"
#include "config_parse.hpp"
#include "user_utils.hh"
#include <llvm/Support/CommandLine.h>
#include <chrono> 

std::string _curr_property;
std::string _flightControl;
bool _nota;

void dumpModule(llvm::Module& M, std::string IR_path, bool isTimer = false) {
    std::error_code EC;
    size_t pos = IR_path.rfind(".ll");
    if (pos != std::string::npos) {
        IR_path = IR_path.substr(0, pos);
    }
    std::string temp = "";
    if (isTimer) {
        temp = "_timer.ll";
    }else{
        temp = "_memdump.ll";
    }
    std::string OutputFileName = IR_path + temp;

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
    INFO("Output instrumented IR file: " << OutputFileName);
}

void handleProgramOptions(int argc, char **argv, bool &isTimer, bool &notaFlag) {
    llvm::cl::opt<bool> TimerMode("timer", 
        llvm::cl::desc("Additional instrument time log code, default instrument mem-dump code"));
    llvm::cl::alias TimerModeAlias("t", llvm::cl::desc("Alias for --timer"), llvm::cl::aliasopt(TimerMode));

    // llvm::cl::opt<std::string> TargetFunction("target", 
    //     llvm::cl::desc("Target function/incall's id to instrument"), 
    //     llvm::cl::value_desc("function_id"));

    llvm::cl::opt<bool> NotaFlag("nota", 
        llvm::cl::desc("No supplementing of missing dynamic memory allocation type information"));

    llvm::cl::ParseCommandLineOptions(argc, argv, "LLVM Flight Control Instrumenter\n");

    if (TimerMode) {
        isTimer = true;
    }
    
    if (NotaFlag) {
        notaFlag = true;
        if (isTimer) {
            WARNING("--nota will be ignored when using --timer");
            notaFlag = false;
        }
    }
    
    if (isTimer)
        INFO("Instrument time log code, the output file is suffix with _timer. Processing...");
    else
        INFO("Instrument mem dump code, the output file is suffix with _memdump. Processing...");
}

int main(int argc, char **argv) {
    auto start = std::chrono::high_resolution_clock::now();
    bool isTimer = false;
    bool notaFlag = false;
    handleProgramOptions(argc, argv, isTimer, notaFlag);

    std::string dtmc = utils::getEnv("DTMC");

    if (dtmc == "") {
        ERROR("Please set DTMC environment variable by export DTMC=/path/to/modelCheckingFlightControl");
        std::abort();
    }
    
    std::string IR_config_path = dtmc + "/configs/IR_config.yml";
    IRConfig config = parseConfig(IR_config_path);
    std::string IR_path = dtmc + "/" + config.base.ir;
    _flightControl = config.base.flightControl;
    _curr_property = config.base.property_name;
    // int call_depth = config.base.call_depth;

    std::string IR_OutputPath = utils::getDirectoryPath(IR_path) + "/" + _curr_property + "/" + utils::getFileName(IR_path);
    IR_path = IR_OutputPath;

    llvm::LLVMContext Context;
    llvm::SMDiagnostic Err;
    std::unique_ptr<llvm::Module> M = parseIRFile(IR_path, Err, Context);

    if (!M) {
        llvm::WithColor::error(llvm::errs(), argv[0]) << "Error reading IR file: " << IR_path << "\n";
        Err.print(argv[0], llvm::errs());
        return 1;
    } 

    if (isTimer){
        instrumenter::runTimer(*M);
    }else{
        std::vector<llvm::Function*> entrypoints = utils::convertFunctionNameToFunction(*M, config.base.entrypoints);
        instrumenter::runInputGetter(*M, entrypoints);
    }

    dumpModule(*M, IR_OutputPath, isTimer);
    INFO("instrumenter pass finished!");
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    std::cout << std::fixed << std::setprecision(2)
    << "instrumenter takes: " <<  duration.count()/1000.0 << " seconds." << std::endl;
    return 0;
}