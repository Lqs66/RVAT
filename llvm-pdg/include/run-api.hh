#ifndef RUN_API_HH
#define RUN_API_HH

// 

#include "ProgramDependencyGraph.hh"
#include "GraphWriter.hh"

using namespace pdg;
using namespace llvm;

extern "C" void runPDGPass(Module &M, bool dumping = false){
    PassBuilder PB;
    ModulePassManager MPM;
    ModuleAnalysisManager MAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    LoopAnalysisManager LAM;
    FunctionPassManager FPM;

    // Register analysis passes with the managers
    PB.registerModuleAnalyses(MAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerLoopAnalyses(LAM);

    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // Build the pass pipeline
    MPM.addPass(IndirectCallPass());
    MPM.addPass(DataDependencyGraph());
    FPM.addPass(ControlDependencyGraph());
    MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
    MPM.addPass(ProgramDependencyGraph(dumping));
    // legacy::PassManager lpm;
    // lpm.add(createCFGPrinterLegacyPassPass());
    // Run the passes
    MPM.run(M, MAM);
}


#endif