//===- ExecutionEngine.h - Abstract Execution Engine Interface --*- C++ -*-===//
//
// This file defines the abstract interface that implements execution support
// for LLVM.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_EXECUTIONENGINE_H
#define LLVM_EXECUTIONENGINE_EXECUTIONENGINE_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Mutex.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;

namespace uppllvm {
struct GenericValue;
class CFAInstruction;

/// Helper class for helping synchronize access to the global address map
/// table.  Access to this class should be serialized under a mutex.
class ExecutionEngineState {
public:
  using GlobalAddressMapTy = StringMap<uint64_t>;

private:
  /// GlobalAddressMap - A mapping between LLVM global symbol names values and
  /// their actualized version...
  GlobalAddressMapTy GlobalAddressMap;

  /// GlobalAddressReverseMap - This is the reverse mapping of GlobalAddressMap,
  /// used to convert raw addresses into the LLVM global value that is emitted
  /// at the address.  This map is not computed unless getGlobalValueAtAddress
  /// is called at some point.
  std::map<uint64_t, std::string> GlobalAddressReverseMap;

public:
  GlobalAddressMapTy &getGlobalAddressMap() {
    return GlobalAddressMap;
  }

  std::map<uint64_t, std::string> &getGlobalAddressReverseMap() {
    return GlobalAddressReverseMap;
  }

  /// Erase an entry from the mapping table.
  ///
  /// \returns The address that \p ToUnmap was happed to.
  uint64_t RemoveMapping(StringRef Name);
};

using FunctionCreator = std::function<void *(const std::string &)>;

/// Abstract interface for implementation execution of LLVM modules,
/// designed to support both interpreter and just-in-time (JIT) compiler
/// implementations.
class ExecutionEngine {
  /// The state object holding the global address mapping, which must be
  /// accessed synchronously.
  ExecutionEngineState EEState;

  /// The target data for the platform for which execution is being performed.
  const DataLayout DL;

  /// Whether the interpreter should verify IR modules during compilation.
  bool VerifyModules;

  friend class EngineBuilder;  // To allow access to InterpCtor.

protected:
  /// The list of Modules that we are interpreting.
  SmallVector<std::unique_ptr<Module>, 1> Modules;

  /// getMemoryforGV - Allocate memory for a global variable.
  virtual char *getMemoryForGV(const GlobalVariable *GV);

  static ExecutionEngine *(*InterpCtor)(std::unique_ptr<Module> M,
                                        std::string *ErrorStr);

  /// LazyFunctionCreator - If an unknown function is needed, this function
  /// pointer is invoked to create it.  If this returns null, the interpreter will
  /// abort.
  FunctionCreator LazyFunctionCreator;

  /// getMangledName - Get mangled name.
  std::string getMangledName(const llvm::GlobalValue *GV);

  std::string ErrMsg;

public:
  /// lock - This lock protects the ExecutionEngine class. It must
  /// be held while changing the internal state.
  sys::Mutex lock;

  //===--------------------------------------------------------------------===//
  //  ExecutionEngine Startup
  //===--------------------------------------------------------------------===//

  virtual ~ExecutionEngine();

  /// Add a Module to the list of modules that we can interpret.
  virtual void addModule(std::unique_ptr<Module> M) {
    Modules.push_back(std::move(M));
  }

  llvm::Module* getModuleAtIndex(unsigned Index) {
    assert(Index < Modules.size() && "Module index out of range!");
    return Modules[Index].get();
  }

  void getMemoryAndMapGV(GlobalVariable *GV);

  const DataLayout &getDataLayout() const { return DL; }

  /// removeModule - Removes a Module from the list of modules, but does not
  /// free the module's memory. Returns true if M is found, in which case the
  /// caller assumes responsibility for deleting the module.
  //
  // FIXME: This stealth ownership transfer is horrible. This will probably be
  //        fixed by deleting ExecutionEngine.
  virtual bool removeModule(Module *M);

  /// FindFunctionNamed - Search all of the active modules to find the function that
  /// defines FnName.  This is very slow operation and shouldn't be used for
  /// general code.
  virtual Function *FindFunctionNamed(StringRef FnName);

  /// FindGlobalVariableNamed - Search all of the active modules to find the global variable
  /// that defines Name.  This is very slow operation and shouldn't be used for
  /// general code.
  virtual GlobalVariable *FindGlobalVariableNamed(StringRef Name, bool AllowInternal = false);

  /// runFunction - Execute the specified function with the specified arguments,
  /// and return the result.
  virtual GenericValue runFunction(Function *F,
                                   ArrayRef<GenericValue> ArgValues) = 0;

  /// getPointerToNamedFunction - This method returns the address of the
  /// specified function by using the dlsym function call.  As such it is only
  /// useful for resolving library symbols, not code generated symbols.
  ///
  /// If AbortOnFailure is false and no function with the given name is
  /// found, this function silently returns a null pointer. Otherwise,
  /// it prints a message to stderr and aborts.
  virtual void *getPointerToNamedFunction(StringRef Name,
                                          bool AbortOnFailure = true) = 0;

  /// Returns true if an error has been recorded.
  bool hasError() const { return !ErrMsg.empty(); }

  /// Clear the error message.
  void clearErrorMessage() { ErrMsg.clear(); }

  /// Returns the most recent error message.
  const std::string &getErrorMessage() const { return ErrMsg; }

  /// runStaticConstructorsDestructors - This method is used to execute all of
  /// the static constructors or destructors for a program.
  ///
  /// \param isDtors - Run the destructors instead of constructors.
  virtual void runStaticConstructorsDestructors(bool isDtors);

  /// This method is used to execute all of the static constructors or
  /// destructors for a particular module.
  ///
  /// \param isDtors - Run the destructors instead of constructors.
  void runStaticConstructorsDestructors(Module &module, bool isDtors);


  /// runFunctionAsMain - This is a helper function which wraps runFunction to
  /// handle the common task of starting up main with the specified argc, argv,
  /// and envp parameters.
  int runFunctionAsMain(Function *Fn, const std::vector<std::string> &argv,
                        const char * const * envp);


  /// addGlobalMapping - Tell the execution engine that the specified global is
  /// at the specified location.  This is used internally as functions are JIT'd
  /// and as global variables are laid out in memory.  It can and should also be
  /// used by clients of the EE that want to have an LLVM global overlay
  /// existing data in memory. Values to be mapped should be named, and have
  /// external or weak linkage. Mappings are automatically removed when their
  /// GlobalValue is destroyed.
  void addGlobalMapping(const llvm::GlobalValue *GV, void *Addr);
  void addGlobalMapping(StringRef Name, uint64_t Addr);

  /// clearAllGlobalMappings - Clear all global mappings and start over again,
  /// for use in dynamic compilation scenarios to move globals.
  void clearAllGlobalMappings();

  /// clearGlobalMappingsFromModule - Clear all global mappings that came from a
  /// particular module, because it has been removed from the JIT.
  void clearGlobalMappingsFromModule(Module *M);

  /// updateGlobalMapping - Replace an existing mapping for GV with a new
  /// address.  This updates both maps as required.  If "Addr" is null, the
  /// entry for the global is removed from the mappings.  This returns the old
  /// value of the pointer, or null if it was not in the map.
  uint64_t updateGlobalMapping(const llvm::GlobalValue *GV, void *Addr);
  uint64_t updateGlobalMapping(StringRef Name, uint64_t Addr);

  /// getAddressToGlobalIfAvailable - This returns the address of the specified
  /// global symbol.
  uint64_t getAddressToGlobalIfAvailable(StringRef S);

  /// getPointerToGlobalIfAvailable - This returns the address of the specified
  /// global value if it is has already been codegen'd, otherwise it returns
  /// null.
  void *getPointerToGlobalIfAvailable(StringRef S);
  void *getPointerToGlobalIfAvailable(const llvm::GlobalValue *GV);

  /// getPointerToGlobal - This returns the address of the specified global
  /// value. This may involve code generation if it's a function.
  void *getPointerToGlobal(const llvm::GlobalValue *GV);

  /// getPointerToFunction - The different EE's represent function bodies in
  /// different ways.  They should each implement this to say what a function
  /// pointer should look like.  When F is destroyed, the ExecutionEngine will
  /// remove its global mapping and free any machine code.  Be sure no threads
  /// are running inside F when that happens.
  virtual void *getPointerToFunction(Function *F) = 0;

  /// getPointerToFunctionOrStub - If the specified function has been
  /// code-gen'd, return a pointer to the function.  If not, compile it, or use
  /// a stub to implement lazy compilation if available.  See
  /// getPointerToFunction for the requirements on destroying F.
  virtual void *getPointerToFunctionOrStub(Function *F) {
    // Default implementation, just codegen the function.
    return getPointerToFunction(F);
  }

  /// getGlobalValueAtAddress - Return the LLVM global value object that starts
  /// at the specified address.
  const llvm::GlobalValue *getGlobalValueAtAddress(void *Addr);

  /// StoreValueToMemory - Stores the data in Val of type Ty at address Ptr.
  /// Ptr is the address of the memory at which to store Val, cast to
  /// GenericValue *.  It is not a pointer to a GenericValue containing the
  /// address at which to store Val.
  void StoreValueToMemory(const GenericValue &Val, GenericValue *Ptr,
                          Type *Ty);

  void LoadValueFromMemory(GenericValue &Result, GenericValue *Ptr,
                          Type *Ty);

  void InitializeMemory(const Constant *Init, void *Addr);

  /// getOrEmitGlobalVariable - Return the address of the specified global
  /// variable, possibly emitting it to memory if needed.  This is used by the
  /// Emitter.
  virtual void *getOrEmitGlobalVariable(const GlobalVariable *GV) {
    return getPointerToGlobal((const llvm::GlobalValue *)GV);
  }

  /// Enable/Disable IR module verification.
  ///
  /// Note: Module verification is enabled by default in Debug builds, and
  /// disabled by default in Release. Use this method to override the default.
  void setVerifyModules(bool Verify) {
    VerifyModules = Verify;
  }
  bool getVerifyModules() const {
    return VerifyModules;
  }

  /// runInstrs - Execute a vector of CFA instructions
  /// This is a virtual function that should be implemented by derived classes
  virtual void runInstrs(std::vector<CFAInstruction>& instructions) {
    llvm_unreachable("runInstrs not implemented in base ExecutionEngine");
  }

  /// InstallLazyFunctionCreator - If an unknown function is needed, the
  /// specified function pointer is invoked to create it.  If it returns null,
  /// the JIT will abort.
  void InstallLazyFunctionCreator(FunctionCreator C) {
    LazyFunctionCreator = std::move(C);
  }

protected:
  ExecutionEngine(DataLayout DL) : DL(std::move(DL)) {}
  explicit ExecutionEngine(DataLayout DL, std::unique_ptr<Module> M);
  explicit ExecutionEngine(std::unique_ptr<Module> M);

  void emitGlobals();

  void emitGlobalVariable(const GlobalVariable *GV);

  GenericValue getConstantValue(const Constant *C);

private:
  void Init(std::unique_ptr<Module> M);
};

namespace EngineKind {

  /// Engine kind for interpreter only
  enum Kind {
    Interpreter = 0x1
  };

} // end namespace EngineKind

/// Builder class for Interpreter ExecutionEngine. Use this by stack-allocating a builder,
/// chaining the various set* methods, and terminating it with a .create() call.
class EngineBuilder {
private:
  std::unique_ptr<Module> M;
  std::string *ErrorStr;
  bool VerifyModules;

public:
  /// Default constructor for EngineBuilder.
  EngineBuilder();

  /// Constructor for EngineBuilder.
  EngineBuilder(std::unique_ptr<Module> M);

  ~EngineBuilder();

  /// setErrorStr - Set the error string to write to on error.  This option
  /// defaults to NULL.
  EngineBuilder &setErrorStr(std::string *e) {
    ErrorStr = e;
    return *this;
  }

  /// setVerifyModules - Set whether the interpreter should verify
  /// IR modules during execution.
  EngineBuilder &setVerifyModules(bool Verify) {
    VerifyModules = Verify;
    return *this;
  }

  ExecutionEngine *create();
};

} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_EXECUTIONENGINE_H
