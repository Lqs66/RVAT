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
#include "llvm/IR/GlobalVariable.h"
#include <cerrno>
#include <string>
#include <vector>
#include <memory>
#include <map>

#include "modelInputs.pb.h"
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "config_parse.hpp"
#include "macro.hh"
#include "user_utils.hh"

#include "ExecutionEngine/GenericValue.h"
#include "Interpreter/InstrsInterpreter.h"
#include "Interpreter/Interpreter.h"

// Map of function names to their stub return values
// Key: function name, Value: integer return value (for integer return types)
// Functions in this map will be stubbed to return the specified value. Then the
// function body will be replaced with a simple 'ret' instruction.
static const std::map<std::string, int> STUB_FUNCTIONS = {
    {"_ZN3GCS10send_textvE12MAV_SEVERITYPKcSt9__va_listh", 0},
    {"mavlink_vasprintf", 0},
    {"px4_log_modulename", 0}
};

extern "C" {

    // extern std::string _property;
    // extern std::unique_ptr<uppllvm::ExecutionEngine> EE_ptr;
    // extern std::vector<std::unique_ptr<uppllvm::CFAModel>> _CFAModels;
    // extern uppllvm::CFAModel* _entryPointCFAModel;

    std::unique_ptr<uppllvm::ExecutionEngine> initInterpreter();

    void printModule();
    
    // get the number of locations in CFAModel
    int nbOfLocations ();

    // get the number of outgoing edges in the location
    int nbOfOutgoingEdges (int loc);

    // According to the loc id and the outgoing edge index, get the target location id
    int getSuccLocID (int loc, int e);

    // get the initial location id
    int getInitLocationID ();

    // Get the name of the location
    const char* getNameOfLocation (int loc);

    // Check if the edge has a conditional branch variable
    bool hasGuard(int l, int e);

    // Evaluate the condition branch variable of the edge
    int evaluateGuard(int l, int e);

    // Get the edge type (0=SEQUENTIAL, 1=BRANCH_TRUE, 2=BRANCH_FALSE, 3=RETURN, 4=CALL)
    int getEdgeType(int l, int e);

    // Execute the edge's instructions
    void executeEdge (int loc, int e);

    int getInstructionCount (int loc, int e);

    /**********************************************************/ 
    /* Comparing the scalar values of two property variables. */
    bool is_true (const char* pName1);

    bool is_false (const char* pName1);
    
    bool bit_is_true (const char* pName1, int bitIndex);

    bool bit_is_false (const char* pName1, int bitIndex);

    bool is_equal_const (const char* pName1, double constVal);

    bool is_not_equal_const (const char* pName1, double constVal);

    bool is_greater_const (const char* pName1, double constVal);

    bool is_greater_or_equal_const (const char* pName1, double constVal);

    bool is_less_const (const char* pName1, double constVal);

    bool is_less_or_equal_const (const char* pName1, double constVal);

    bool is_equal (const char* pName1, const char* pName2);

    bool is_not_equal (const char* pName1, const char* pName2);

    bool is_greater (const char* pName1, const char* pName2);

    bool is_greater_or_equal (const char* pName1, const char* pName2);

    bool is_less (const char* pName1, const char* pName2);

    bool is_less_or_equal (const char* pName1, const char* pName2);
    /**********************************************************/ 

    void buildCFAModels();
    
    // Initialize the model by loading model input file.
    // We depend on the environment variable MODEL_INPUT_PATH to get the file path.
    void initModel ();

    void initMemory (const char* modelInputPath);

    void initParams ();

    void showMemory();

    void showEntryArgs();

#ifdef DEBUG
    void showQueries();
#endif
}