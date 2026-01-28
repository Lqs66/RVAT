#include "InstrsInterpreter.h"
using namespace llvm;
using namespace uppllvm;

InstrsInterpreter::InstrsInterpreter(std::unique_ptr<Module> M)
    : Interpreter(std::move(M)) {
    // initializeExternalFunctions();
    // emitGlobals();
}

InstrsInterpreter::~InstrsInterpreter() {
}

ExecutionEngine *InstrsInterpreter::create(std::unique_ptr<Module> M,
    std::string *ErrorStr) {
    // Tell this Module to materialize everything and release the GVMaterializer.
    if (Error Err = M->materializeAll()) {
        std::string Msg;
        handleAllErrors(std::move(Err), [&](ErrorInfoBase &EIB) {
        Msg = EIB.message();
        });
        if (ErrorStr)
        *ErrorStr = Msg;
        // We got an error, just return 0
        return nullptr;
    }
    return new InstrsInterpreter(std::move(M));
}