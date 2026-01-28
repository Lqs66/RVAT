//===-- InterpreterR.cpp - LLVM IR Interpreter Main ----------------------===//
//
// InterpreterR - LLVM IR Interpreter
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Transforms/Utils.h"
#include <cerrno>
#include <string>
#include <vector>
#include <memory>


#include "ExecutionEngine/GenericValue.h"
#include "Interpreter/Interpreter.h"

namespace cl = llvm::cl;

using namespace llvm;
using namespace uppllvm;
//===----------------------------------------------------------------------===//
// Command line options
//===----------------------------------------------------------------------===//

cl::opt<std::string> InputFile(cl::desc("<input bitcode>"), cl::Positional,
                                 cl::init("-"));

cl::list<std::string>
InputArgv(cl::ConsumeAfter, cl::desc("<program arguments>..."));

// remove call @llvm.type.test and @llvm.assume
void removeTypeTestAndAssume(llvm::Module &M){
    for (auto& F : M){
        for (auto& BB : F){
            for (auto I = BB.begin(), E = BB.end(); I != E; ) {
                if (auto* call = llvm::dyn_cast<llvm::CallBase>(&*I)){
                    if (!call->isIndirectCall()){
                        if (call->getCalledFunction()->getName() == "llvm.type.test" || call->getCalledFunction()->getName() == "llvm.assume"){
                            I = call->eraseFromParent();
                            continue;
                        }
                    }
                }
                ++I;
            }
        }
    }
}

//===----------------------------------------------------------------------===//
// Main function
//===----------------------------------------------------------------------===//

int main(int argc, char **argv, char * const *envp) {
  InitLLVM X(argc, argv);
  ExitOnError ExitOnErr;

  if (argc > 1)
    ExitOnErr.setBanner(std::string(argv[0]) + ": ");

  cl::ParseCommandLineOptions(argc, argv,
                              "llvm interpreter & dynamic compiler\n");

  LLVMContext Context;
  
  // Load the bitcode...
  SMDiagnostic Err;
  std::unique_ptr<Module> Owner = parseIRFile(InputFile, Err, Context);
  Module *Mod = Owner.get();
  if (!Mod) {
    Err.print(argv[0], errs());
    return 1;
  }

  // Load the whole bitcode file eagerly.
  {
    // Use *argv instead of argv[0] to work around a wrong GCC warning.
    ExitOnError ExitOnErr(std::string(*argv) +
                          ": bitcode didn't read correctly: ");
    ExitOnErr(Mod->materializeAll());
  }

  removeTypeTestAndAssume(*Mod);
  StripDebugInfo(*Mod);

  // Load system C library to make functions like sysconf, printf, etc. available
  // This is necessary for the interpreter to resolve external function symbols
  std::string LibErr;
  if (sys::DynamicLibrary::LoadLibraryPermanently(nullptr, &LibErr)) {
    WithColor::warning(errs(), argv[0]) 
        << "Could not load program symbols: " << LibErr << "\n";
  }


  // Create the interpreter execution engine
  std::string ErrorMsg;
  std::unique_ptr<ExecutionEngine> EE(Interpreter::create(std::move(Owner), &ErrorMsg));
  if (!EE) {
    if (!ErrorMsg.empty())
      WithColor::error(errs(), argv[0])
          << "error creating EE: " << ErrorMsg << "\n";
    else
      WithColor::error(errs(), argv[0]) << "unknown error creating EE!\n";
    exit(1);
  }

  std::string EntryFunc = "main";

  Function *EntryFn = Mod->getFunction(EntryFunc);
  if (!EntryFn) {
    WithColor::error(errs(), argv[0])
        << '\'' << EntryFunc << "\' function not found in module.\n";
    return -1;
  }

  if (EntryFunc == "main") {
    // Call the main function from M as if its signature were:
    //   int main (int argc, char **argv, const char **envp)
    // using the contents of Args to determine argc & argv, and the contents of
    // EnvVars to determine envp.
    //

    InputArgv.insert(InputArgv.begin(), InputFile);

    // Reset errno to zero on entry to main.
    errno = 0;

    int Result = -1;

    // If the program doesn't explicitly call exit, we will need the Exit
    // function later on to make an explicit call, so get the function now.
    FunctionCallee Exit = Mod->getOrInsertFunction(
        "exit", Type::getVoidTy(Context), Type::getInt32Ty(Context));

    // Run static constructors.
    EE->runStaticConstructorsDestructors(false);

    // // Run main.
    // Result = EE->runFunctionAsMain(EntryFn, InputArgv, envp);

    // // Run static destructors.
    EE->runStaticConstructorsDestructors(true);

    // If the program didn't call exit explicitly, we should call it now.
    // This ensures that any atexit handlers get called correctly.
    if (Function *ExitF =
            dyn_cast<Function>(Exit.getCallee()->stripPointerCasts())) {
      if (ExitF->getFunctionType() == Exit.getFunctionType()) {
        std::vector<GenericValue> Args;
        GenericValue ResultGV;
        ResultGV.IntVal = APInt(32, Result);
        Args.push_back(ResultGV);
        EE->runFunction(ExitF, Args);
        WithColor::error(errs(), argv[0])
            << "exit(" << Result << ") returned!\n";
        abort();
      }
    }
  }
  else {
    std::vector<GenericValue> args;
    EE->runStaticConstructorsDestructors(false);
    EE->runFunction(EntryFn, args);
  }

  WithColor::error(errs(), argv[0]) << "exit defined with wrong prototype!\n";
  abort();
}