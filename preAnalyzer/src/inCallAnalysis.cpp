#include "vCallAnalysis.hh"
#include "inCallUtil.hh"
#include "svfAndersen.hh"
#include "user_utils.hh"
#include "Analyzer.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/WithColor.h>

#include <boost/program_options.hpp>

#include <chrono>

// std::map<int, incall::IndirectCallEdge*> incallEdges;
std::map<int, incall::IndirectCallEdge> TypeMDIncallEdges;
std::map<int, incall::IndirectCallEdge> SVFIncallEdges;
std::map<int, incall::IndirectCallEdge> DeepTypeIncallEdges;

void incall::dumpIndirectCalls(std::string output, std::map<int, incall::IndirectCallEdge> &incallEdges){
    llvm::sys::fs::create_directories(llvm::sys::path::parent_path(output));
    
    // dump to yaml file
    std::error_code EC;
    llvm::raw_fd_ostream OutFile(output, EC, llvm::sys::fs::OF_None);
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message() << "\n";
        return;
    }
    OutFile << "indirectCalls:\n";
    for (auto &pair : incallEdges){
        OutFile << " - inCallID: " << pair.first << "\n";
        if (pair.second.isVirtual)
            OutFile << "   isvcall: true\n";
        else
            OutFile << "   isvcall: false\n";
        OutFile << "   targets:\n";
        for (auto &target : pair.second.candidates){
            OutFile << "    - " << target->getName() << "\n";
        }
    }
}

void handleProgramOptions(int argc, char **argv, std::string &IR_path, bool &isVCall, bool &isSVF, bool &isDeepType, bool &isALL) {
    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Show me help message")
        ("input,i", boost::program_options::value<std::string>(&IR_path), "Specify the input IR file")
        ("vcall,v", "Run virtual call analysis, default is false")
        ("svf,s", "Run SVF Andersen analysis, default is false")
        ("deeptype,d", "Run DeepType analysis, default is false")
        ("all,a", "Run all analysis, default is false");

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    try {
        boost::program_options::notify(vm);
    } catch (const boost::program_options::unknown_option& e) {
        ERROR("Unknown option: " << e.what() << "\n");
        std::cout << desc << "\n";
        exit(1);
    }

    if (vm.count("help")){
        std::cout << desc << "\n";
        exit(0);
    }
    if (vm.count("input")) {
        IR_path = vm["input"].as<std::string>();
    }
    if (vm.count("vcall")) {
        isVCall = true;
    }
    if (vm.count("svf")) {
        isSVF = true;
    }
    if (vm.count("deeptype")) {
        isDeepType = true;
    }
    if (vm.count("all")) {
        isALL = true;
    }
    if (!isVCall && !isSVF && !isDeepType && !isALL){
        ERROR("Please specify an analysis!");
        std::cout << desc << "\n";
        exit(1);
    }
    if ((isVCall || isSVF || isDeepType) && isALL){
        ERROR("Individual analysis options cannot be used with all analysis!");
        std::cout << desc << "\n";
        exit(1);
    }
}

int main(int argc, char **argv) {
    auto start = std::chrono::high_resolution_clock::now();
    bool isVCall = false;
    bool isSVF = false;
    bool isDeepType = false;
    bool isALL = false;
    std::string dtmc = utils::getEnv("DTMC");
    if (dtmc.empty()) {
        ERROR("DTMC environment variable is not set!");
        return 1;
    }
    std::string IR_path = dtmc + "/verifyDataBase/ir_and_elf/arducopter_sitl_pi64.ll";
    handleProgramOptions(argc, argv, IR_path, isVCall, isSVF, isDeepType, isALL);

    llvm::LLVMContext context;
    llvm::SMDiagnostic error;
    std::error_code EC;

    if (IR_path == dtmc + "/verifyDataBase/ir_and_elf/arducopter_sitl_pi64.ll"){
        INFO("Parsing default IR file: " << IR_path << " ...");
    }else{
        INFO("Parsing IR file: " << IR_path << " ...");
    }

    if (isVCall || isALL) {
        INFO("Running virtual call analysis...");
        std::unique_ptr<llvm::Module> module = parseIRFile(IR_path, error, context);
        
        if (!module) {
            error.print("Error parsing IR file", llvm::errs());
            return 1;
        }
        vcall::runVirtualCallAnalysis(*module);
        incall::dumpIndirectCalls(dtmc + "/preAnalyzer/build/incalls/TypeMDInCalls.yml", TypeMDIncallEdges);
    }
    
    if (isSVF || isALL) {
        INFO("Running SVF Andersen analysis...");
        std::unique_ptr<llvm::Module> module = parseIRFile(IR_path, error, context);
        
        if (!module) {
            error.print("Error parsing IR file", llvm::errs());
            return 1;
        }
        svfAndersen::runSVFAndersen(*module);
        incall::dumpIndirectCalls(dtmc + "/preAnalyzer/build/incalls/SVFInCalls.yml", SVFIncallEdges);
    }
    
    if (isDeepType || isALL) {
        INFO("Running DeepType analysis...");
        context.setOpaquePointers(false);
        std::unique_ptr<llvm::Module> module = parseIRFile(IR_path, error, context);
        
        if (!module) {
            error.print("Error parsing IR file", llvm::errs());
            return 1;
        }
        DeepType::runDeepTypeAnalysis(*module);
        incall::dumpIndirectCalls(dtmc + "/preAnalyzer/build/incalls/DeepTypeInCalls.yml", DeepTypeIncallEdges);
    }
        
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;    
    INFO("Analysis finished in " << duration.count() << " seconds.");
    return 0;
}