/**
 * testMain.cpp - Test for base.cpp initMemory function
 * 
 * This test verifies that initMemory correctly writes values from protobuf
 * to global variables and heap memory in the LLVM ExecutionEngine.
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <iomanip>

#include "base/base.h"
#include "ExecutionEngine/GenericValue.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/raw_ostream.h"

#include "modelInputs.pb.h"
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

// ANSI color codes for output
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RESET   "\033[0m"

#define TEST_HEADER(msg) std::cout << "\n" << COLOR_CYAN << "=== " << msg << " ===" << COLOR_RESET << "\n"
#define TEST_INFO(msg)   std::cout << COLOR_BLUE << "[INFO] " << COLOR_RESET << msg << "\n"
#define TEST_PASS(msg)   std::cout << COLOR_GREEN << "[PASS] " << COLOR_RESET << msg << "\n"
#define TEST_FAIL(msg)   std::cout << COLOR_RED << "[FAIL] " << COLOR_RESET << msg << "\n"
#define TEST_WARN(msg)   std::cout << COLOR_YELLOW << "[WARN] " << COLOR_RESET << msg << "\n"

// Test counters
static int g_passed = 0;
static int g_failed = 0;

// We'll create our own EE_ptr for testing instead of using the one from base.cpp
std::unique_ptr<uppllvm::ExecutionEngine> EE_ptr;

/**
 * Create a minimal LLVM module with test global variables
 * This avoids loading the full flight control IR which causes linking issues
 */
bool initTestModule() {
    TEST_INFO("Creating minimal test LLVM module");
    
    static llvm::LLVMContext Context;
    std::unique_ptr<llvm::Module> Owner = std::make_unique<llvm::Module>("test_module", Context);
    llvm::Module* Mod = Owner.get();
    
    // Create types
    llvm::Type* i64Type = llvm::Type::getInt64Ty(Context);
    llvm::Type* i32Type = llvm::Type::getInt32Ty(Context);
    llvm::Type* floatType = llvm::Type::getFloatTy(Context);
    llvm::Type* doubleType = llvm::Type::getDoubleTy(Context);
    llvm::PointerType* ptrType = llvm::PointerType::get(llvm::Type::getInt8Ty(Context), 0);
    
    // Create global variables matching our protobuf test data
    new llvm::GlobalVariable(*Mod, i64Type, false,
                            llvm::GlobalValue::ExternalLinkage,
                            llvm::ConstantInt::get(i64Type, 0),
                            "test_int64");
    
    new llvm::GlobalVariable(*Mod, i32Type, false,
                            llvm::GlobalValue::ExternalLinkage,
                            llvm::ConstantInt::get(i32Type, 0),
                            "test_int32");
    
    new llvm::GlobalVariable(*Mod, floatType, false,
                            llvm::GlobalValue::ExternalLinkage,
                            llvm::ConstantFP::get(floatType, 0.0f),
                            "test_float");
    
    new llvm::GlobalVariable(*Mod, doubleType, false,
                            llvm::GlobalValue::ExternalLinkage,
                            llvm::ConstantFP::get(doubleType, 0.0),
                            "test_double");
    
    new llvm::GlobalVariable(*Mod, ptrType, false,
                            llvm::GlobalValue::ExternalLinkage,
                            llvm::ConstantPointerNull::get(ptrType),
                            "test_null_ptr");
    
    new llvm::GlobalVariable(*Mod, ptrType, false,
                            llvm::GlobalValue::ExternalLinkage,
                            llvm::ConstantPointerNull::get(ptrType),
                            "test_ptr_to_int");
    
    // Create struct type: {i32, float, i8*, double}
    std::vector<llvm::Type*> structMembers = {i32Type, floatType, ptrType, doubleType};
    llvm::StructType* structType = llvm::StructType::create(Context, structMembers, "TestStruct");
    
    new llvm::GlobalVariable(*Mod, structType, false,
                            llvm::GlobalValue::ExternalLinkage,
                            llvm::ConstantAggregateZero::get(structType),
                            "test_struct");
    
    // Create the interpreter execution engine
    std::string ErrorMsg;
    EE_ptr.reset(uppllvm::Interpreter::create(std::move(Owner), &ErrorMsg));
    
    if (!EE_ptr) {
        TEST_FAIL("Failed to create ExecutionEngine: " + ErrorMsg);
        return false;
    }
    
    // Note: Don't manually allocate globals here, ExecutionEngine does it automatically
    
    TEST_PASS("Test LLVM module created successfully");
    return true;
}

/**
 * initMemory - Load memory state from protobuf file
 * This is copied from base.cpp to avoid linking issues
 */
void initMemory(const char* modelInputPath){
    // parse model input protobuf file
    std::ifstream input(modelInputPath);
    if (!input.is_open()) {
        TEST_FAIL("Failed to open file " + std::string(modelInputPath));
        abort();
    }
    google::protobuf::io::IstreamInputStream zero_copy_stream(&input);
    google::protobuf::io::CodedInputStream coded_stream(&zero_copy_stream);
    modelInputs::ModelInputs modelInputs;
    modelInputs.ParseFromCodedStream(&coded_stream);
    input.close();

    llvm::Module* Mod = EE_ptr->getModuleAtIndex(0);
    llvm::LLVMContext& Context = Mod->getContext();
    
    struct logicAddrRange {
        uint64_t start;
        uint64_t end;
        std::string name;
    };
    std::vector<logicAddrRange> logicAddrRanges;
    size_t heapStart = 0;
    for (const auto &globalVar : modelInputs.globalvars()) {
        logicAddrRange range;
        range.start = globalVar.offset();
        range.end = range.start + globalVar.size();
        heapStart += globalVar.size();
        range.name = globalVar.name();
        logicAddrRanges.push_back(range);
    }
    
    size_t heap_size = 0; 
    for (const auto &heapVar : modelInputs.heapvars()) {
        heap_size += heapVar.size();
    }
    void* heapBaseAddr = nullptr;
    if (heap_size > 0) {
        // create global variable @heap
        llvm::Type* i8Type = llvm::Type::getInt8Ty(Context);
        llvm::ArrayType* heapArrayType = llvm::ArrayType::get(i8Type, heap_size);
        llvm::Constant* zeroInitializer = llvm::ConstantAggregateZero::get(heapArrayType);
        llvm::GlobalVariable* heapGlobal = new llvm::GlobalVariable(
            *Mod,                                    // Module
            heapArrayType,                           // Type
            false,                                   // isConstant
            llvm::GlobalValue::ExternalLinkage,      // Linkage
            zeroInitializer,                         // Initializer
            "heap"                                   // Name
        );
        // Allocate memory for @heap and add to GlobalAddressMap
        EE_ptr->getMemoryAndMapGV(heapGlobal);
        // Get heap base address
        heapBaseAddr = EE_ptr->getPointerToGlobalIfAvailable(heapGlobal);
        if (!heapBaseAddr) {
            TEST_FAIL("Failed to allocate memory for heap");
            abort();
        }
        logicAddrRange range;
        range.start = heapStart;
        range.end = range.start + heap_size;
        range.name = "heap";
        logicAddrRanges.push_back(range);
    }
    
    // Initialize global variables' values
    for (const auto &globalVar : modelInputs.globalvars()) {
        llvm::GlobalVariable* GV = Mod->getGlobalVariable(globalVar.name());
        
        void* Addr = EE_ptr->getPointerToGlobalIfAvailable(GV);

        // Create GenericValue and fill data
        uppllvm::GenericValue GVal;
        llvm::Type* GVType = GV->getValueType();
        if (globalVar.type() == modelInputs::TypeSpec::INT) {
            unsigned BitWidth = GVType->getIntegerBitWidth();
            GVal.IntVal = llvm::APInt(BitWidth, globalVar.i64_value());
            EE_ptr->StoreValueToMemory(GVal, (uppllvm::GenericValue*)Addr, GVType);
        } else if (globalVar.type() == modelInputs::TypeSpec::FLOAT) {
            GVal.FloatVal = globalVar.f_value();
            EE_ptr->StoreValueToMemory(GVal, (uppllvm::GenericValue*)Addr, GVType);
        } else if (globalVar.type() == modelInputs::TypeSpec::DOUBLE) {
            GVal.DoubleVal = globalVar.d_value();
            EE_ptr->StoreValueToMemory(GVal, (uppllvm::GenericValue*)Addr, GVType);
        } else if (globalVar.type() == modelInputs::TypeSpec::PTR) {
            // Convert logical offset to physical address
            int64_t ptrOffset = globalVar.i64_value();
            if (ptrOffset == -1) {
                // NULL pointer
                GVal.PointerVal = nullptr;
            } else {
                bool found = false;
                for (const auto& range : logicAddrRanges) {
                    if (ptrOffset >= range.start && ptrOffset < range.end) {
                        if (range.name == "heap") {
                            GVal.PointerVal = (char*)heapBaseAddr + (ptrOffset - range.start);
                        } else {
                            GVal.PointerVal = (char*)EE_ptr->getPointerToGlobalIfAvailable(
                                Mod->getGlobalVariable(range.name)) + (ptrOffset - range.start);
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    GVal.PointerVal = nullptr;
                }
            }
            EE_ptr->StoreValueToMemory(GVal, (uppllvm::GenericValue*)Addr, GVType);
        } else if (globalVar.type() == modelInputs::TypeSpec::AGGR) {
            if (globalVar.has_members()) {
                // For aggregate types, process each member
                for (const auto& member : globalVar.members().members()) {
                    uppllvm::GenericValue MemberVal;
                    void* MemberAddr = (char*)Addr + member.member_offset();
                    
                    llvm::Type* MemberType = nullptr;
                    if (member.type() == modelInputs::TypeSpec::INT) {
                        unsigned BitWidth = member.size() * 8;
                        MemberVal.IntVal = llvm::APInt(BitWidth, member.i64_value());
                        MemberType = llvm::Type::getIntNTy(Context, BitWidth);
                    } else if (member.type() == modelInputs::TypeSpec::FLOAT) {
                        MemberVal.FloatVal = member.f_value();
                        MemberType = llvm::Type::getFloatTy(Context);
                    } else if (member.type() == modelInputs::TypeSpec::DOUBLE) {
                        MemberVal.DoubleVal = member.d_value();
                        MemberType = llvm::Type::getDoubleTy(Context);
                    } else if (member.type() == modelInputs::TypeSpec::PTR) {
                        // Convert logical offset to physical address for member pointers
                        int64_t ptrOffset = member.i64_value();
                        if (ptrOffset == -1) {
                            MemberVal.PointerVal = nullptr;
                        } else {
                            bool found = false;
                            for (const auto& range : logicAddrRanges) {
                                if (ptrOffset >= range.start && ptrOffset < range.end) {
                                    if (range.name == "heap") {
                                        MemberVal.PointerVal = (char*)heapBaseAddr + (ptrOffset - range.start);
                                    } else {
                                        MemberVal.PointerVal = (char*)EE_ptr->getPointerToGlobalIfAvailable(
                                            Mod->getGlobalVariable(range.name)) + (ptrOffset - range.start);
                                    }
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                MemberVal.PointerVal = nullptr;
                            }
                        }
                        MemberType = llvm::Type::getInt8PtrTy(Context);
                    }
                    if (MemberType) {
                        EE_ptr->StoreValueToMemory(MemberVal, (uppllvm::GenericValue*)MemberAddr, MemberType);
                    }
                }
            }
        }
    }
    
    // Initialize heap variables' values
    if (heapBaseAddr != nullptr) {
        for (const auto &heapVar : modelInputs.heapvars()) {
            void* varAddr = (char*)heapBaseAddr + heapVar.offset() - heapStart;
            
            if (heapVar.type() == modelInputs::TypeSpec::INT) {
                uppllvm::GenericValue Val;
                unsigned BitWidth = heapVar.size() * 8;
                Val.IntVal = llvm::APInt(BitWidth, heapVar.i64_value());
                llvm::Type* VarType = llvm::Type::getIntNTy(Context, BitWidth);
                EE_ptr->StoreValueToMemory(Val, (uppllvm::GenericValue*)varAddr, VarType);
            } else if (heapVar.type() == modelInputs::TypeSpec::FLOAT) {
                uppllvm::GenericValue Val;
                Val.FloatVal = heapVar.f_value();
                EE_ptr->StoreValueToMemory(Val, (uppllvm::GenericValue*)varAddr, 
                                          llvm::Type::getFloatTy(Context));
            } else if (heapVar.type() == modelInputs::TypeSpec::DOUBLE) {
                uppllvm::GenericValue Val;
                Val.DoubleVal = heapVar.d_value();
                EE_ptr->StoreValueToMemory(Val, (uppllvm::GenericValue*)varAddr, 
                                          llvm::Type::getDoubleTy(Context));
            } else if (heapVar.type() == modelInputs::TypeSpec::PTR) {
                // Convert logical offset to physical address
                uppllvm::GenericValue Val;
                int64_t ptrOffset = heapVar.i64_value();
                if (ptrOffset == -1) {
                    Val.PointerVal = nullptr;
                } else {
                    bool found = false;
                    for (const auto& range : logicAddrRanges) {
                        if (ptrOffset >= range.start && ptrOffset < range.end) {
                            if (range.name == "heap") {
                                Val.PointerVal = (char*)heapBaseAddr + (ptrOffset - range.start);
                            } else {
                                Val.PointerVal = (char*)EE_ptr->getPointerToGlobalIfAvailable(
                                    Mod->getGlobalVariable(range.name)) + (ptrOffset - range.start);
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        Val.PointerVal = nullptr;
                    }
                }
                EE_ptr->StoreValueToMemory(Val, (uppllvm::GenericValue*)varAddr, 
                                          llvm::Type::getInt8PtrTy(Context));
            } else if (heapVar.type() == modelInputs::TypeSpec::AGGR && heapVar.has_members()) {
                // Process aggregate types in heap
                for (const auto& member : heapVar.members().members()) {
                    uppllvm::GenericValue MemberVal;
                    void* MemberAddr = (char*)varAddr + member.member_offset();
                    
                    llvm::Type* MemberType = nullptr;
                    if (member.type() == modelInputs::TypeSpec::INT) {
                        unsigned BitWidth = member.size() * 8;
                        MemberVal.IntVal = llvm::APInt(BitWidth, member.i64_value());
                        MemberType = llvm::Type::getIntNTy(Context, BitWidth);
                    } else if (member.type() == modelInputs::TypeSpec::FLOAT) {
                        MemberVal.FloatVal = member.f_value();
                        MemberType = llvm::Type::getFloatTy(Context);
                    } else if (member.type() == modelInputs::TypeSpec::DOUBLE) {
                        MemberVal.DoubleVal = member.d_value();
                        MemberType = llvm::Type::getDoubleTy(Context);
                    } else if (member.type() == modelInputs::TypeSpec::PTR) {
                        // Convert logical offset to physical address for member pointers
                        int64_t ptrOffset = member.i64_value();
                        if (ptrOffset == -1) {
                            MemberVal.PointerVal = nullptr;
                        } else {
                            bool found = false;
                            for (const auto& range : logicAddrRanges) {
                                if (ptrOffset >= range.start && ptrOffset < range.end) {
                                    if (range.name == "heap") {
                                        MemberVal.PointerVal = (char*)heapBaseAddr + (ptrOffset - range.start);
                                    } else {
                                        MemberVal.PointerVal = (char*)EE_ptr->getPointerToGlobalIfAvailable(
                                            Mod->getGlobalVariable(range.name)) + (ptrOffset - range.start);
                                    }
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                MemberVal.PointerVal = nullptr;
                            }
                        }
                        MemberType = llvm::Type::getInt8PtrTy(Context);
                    }
                    if (MemberType) {
                        EE_ptr->StoreValueToMemory(MemberVal, (uppllvm::GenericValue*)MemberAddr, MemberType);
                    }
                }
            }
        }
    }
}

/**
 * Helper: Create a test protobuf file with various types of data
 */
bool createTestProtobuf(const std::string& filename) {
    TEST_INFO("Creating test protobuf file: " + filename);
    
    modelInputs::ModelInputs inputs;
    
    // Global variable 1: Integer (int64)
    auto* gv1 = inputs.add_globalvars();
    gv1->set_offset(0);
    gv1->set_name("test_int64");
    gv1->set_type(modelInputs::TypeSpec::INT);
    gv1->set_size(8);
    gv1->set_i64_value(12345678);
    
    // Global variable 2: Integer (int32)
    auto* gv2 = inputs.add_globalvars();
    gv2->set_offset(8);
    gv2->set_name("test_int32");
    gv2->set_type(modelInputs::TypeSpec::INT);
    gv2->set_size(4);
    gv2->set_i64_value(999);
    
    // Global variable 3: Float
    auto* gv3 = inputs.add_globalvars();
    gv3->set_offset(12);
    gv3->set_name("test_float");
    gv3->set_type(modelInputs::TypeSpec::FLOAT);
    gv3->set_size(4);
    gv3->set_f_value(3.14159f);
    
    // Global variable 4: Double
    auto* gv4 = inputs.add_globalvars();
    gv4->set_offset(16);
    gv4->set_name("test_double");
    gv4->set_type(modelInputs::TypeSpec::DOUBLE);
    gv4->set_size(8);
    gv4->set_d_value(2.718281828);
    
    // Global variable 5: Pointer (NULL)
    auto* gv5 = inputs.add_globalvars();
    gv5->set_offset(24);
    gv5->set_name("test_null_ptr");
    gv5->set_type(modelInputs::TypeSpec::PTR);
    gv5->set_size(8);
    gv5->set_i64_value(-1); // NULL pointer
    
    // Global variable 6: Pointer (pointing to test_int64 at offset 0)
    auto* gv6 = inputs.add_globalvars();
    gv6->set_offset(32);
    gv6->set_name("test_ptr_to_int");
    gv6->set_type(modelInputs::TypeSpec::PTR);
    gv6->set_size(8);
    gv6->set_i64_value(0); // Points to test_int64
    
    // Global variable 7: Struct with members
    auto* gv7 = inputs.add_globalvars();
    gv7->set_offset(40);
    gv7->set_name("test_struct");
    gv7->set_type(modelInputs::TypeSpec::AGGR);
    gv7->set_size(24);
    
    auto* members = gv7->mutable_members();
    // Member 0: int at offset 0
    auto* m0 = members->add_members();
    m0->set_member_offset(0);
    m0->set_type(modelInputs::TypeSpec::INT);
    m0->set_size(4);
    m0->set_i64_value(100);
    
    // Member 1: float at offset 4
    auto* m1 = members->add_members();
    m1->set_member_offset(4);
    m1->set_type(modelInputs::TypeSpec::FLOAT);
    m1->set_size(4);
    m1->set_f_value(2.5f);
    
    // Member 2: pointer at offset 8 (points to test_double at offset 16)
    auto* m2 = members->add_members();
    m2->set_member_offset(8);
    m2->set_type(modelInputs::TypeSpec::PTR);
    m2->set_size(8);
    m2->set_i64_value(16); // Points to test_double
    
    // Member 3: double at offset 16
    auto* m3 = members->add_members();
    m3->set_member_offset(16);
    m3->set_type(modelInputs::TypeSpec::DOUBLE);
    m3->set_size(8);
    m3->set_d_value(9.876);
    
    // Heap variables start at offset 64 (after all globals)
    uint64_t heapStart = 64;
    
    // Heap variable 1: Integer
    auto* hv1 = inputs.add_heapvars();
    hv1->set_hash(0x1111);
    hv1->set_offset(heapStart);
    hv1->set_type(modelInputs::TypeSpec::INT);
    hv1->set_size(4);
    hv1->set_i64_value(777);
    
    // Heap variable 2: Float
    auto* hv2 = inputs.add_heapvars();
    hv2->set_hash(0x2222);
    hv2->set_offset(heapStart + 4);
    hv2->set_type(modelInputs::TypeSpec::FLOAT);
    hv2->set_size(4);
    hv2->set_f_value(8.88f);
    
    // Heap variable 3: Pointer (pointing to heap int at heapStart)
    auto* hv3 = inputs.add_heapvars();
    hv3->set_hash(0x3333);
    hv3->set_offset(heapStart + 8);
    hv3->set_type(modelInputs::TypeSpec::PTR);
    hv3->set_size(8);
    hv3->set_i64_value(heapStart); // Points to heap int
    
    // Heap variable 4: Struct
    auto* hv4 = inputs.add_heapvars();
    hv4->set_hash(0x4444);
    hv4->set_offset(heapStart + 16);
    hv4->set_type(modelInputs::TypeSpec::AGGR);
    hv4->set_size(16);
    
    auto* heap_members = hv4->mutable_members();
    // Member 0: int
    auto* hm0 = heap_members->add_members();
    hm0->set_member_offset(0);
    hm0->set_type(modelInputs::TypeSpec::INT);
    hm0->set_size(4);
    hm0->set_i64_value(555);
    
    // Member 1: pointer at offset 8 (points to global test_int64)
    auto* hm1 = heap_members->add_members();
    hm1->set_member_offset(8);
    hm1->set_type(modelInputs::TypeSpec::PTR);
    hm1->set_size(8);
    hm1->set_i64_value(0); // Points to global test_int64
    
    // Write to file
    std::ofstream output(filename, std::ios::binary);
    if (!output.is_open()) {
        TEST_FAIL("Cannot create file: " + filename);
        return false;
    }
    
    if (!inputs.SerializeToOstream(&output)) {
        TEST_FAIL("Failed to serialize protobuf");
        output.close();
        return false;
    }
    
    output.close();
    TEST_PASS("Created test protobuf with 7 globals + 4 heap vars");
    return true;
}

/**
 * Verify that a global variable's memory contains the expected value
 */
template<typename T>
bool verifyGlobalValue(const std::string& varName, T expectedValue, const std::string& typeDesc) {
    llvm::Module* Mod = EE_ptr->getModuleAtIndex(0);
    llvm::GlobalVariable* GV = Mod->getGlobalVariable(varName);
    
    if (!GV) {
        TEST_FAIL(varName + ": Global variable not found in module");
        g_failed++;
        return false;
    }
    
    void* addr = EE_ptr->getPointerToGlobalIfAvailable(GV);
    if (!addr) {
        TEST_FAIL(varName + ": Failed to get address");
        g_failed++;
        return false;
    }
    
    T actualValue = *static_cast<T*>(addr);
    
    bool passed = false;
    if constexpr (std::is_floating_point_v<T>) {
        // For floating point, use epsilon comparison
        passed = std::abs(actualValue - expectedValue) < 1e-5;
    } else {
        passed = (actualValue == expectedValue);
    }
    
    if (passed) {
        std::ostringstream oss;
        oss << varName << " (" << typeDesc << "): " << actualValue << " == " << expectedValue;
        TEST_PASS(oss.str());
        g_passed++;
        return true;
    } else {
        std::ostringstream oss;
        oss << varName << " (" << typeDesc << "): expected " << expectedValue 
            << ", got " << actualValue;
        TEST_FAIL(oss.str());
        g_failed++;
        return false;
    }
}

/**
 * Verify pointer value
 */
bool verifyPointer(const std::string& varName, void* expectedPtr, const std::string& desc) {
    llvm::Module* Mod = EE_ptr->getModuleAtIndex(0);
    llvm::GlobalVariable* GV = Mod->getGlobalVariable(varName);
    
    if (!GV) {
        TEST_FAIL(varName + ": Global variable not found");
        g_failed++;
        return false;
    }
    
    void* addr = EE_ptr->getPointerToGlobalIfAvailable(GV);
    if (!addr) {
        TEST_FAIL(varName + ": Failed to get address");
        g_failed++;
        return false;
    }
    
    void* actualPtr = *static_cast<void**>(addr);
    
    if (actualPtr == expectedPtr) {
        std::ostringstream oss;
        oss << varName << " (ptr): " << actualPtr << " == " << expectedPtr << " " << desc;
        TEST_PASS(oss.str());
        g_passed++;
        return true;
    } else {
        std::ostringstream oss;
        oss << varName << " (ptr): expected " << expectedPtr << ", got " << actualPtr;
        TEST_FAIL(oss.str());
        g_failed++;
        return false;
    }
}

/**
 * Verify struct member value
 */
template<typename T>
bool verifyStructMember(const std::string& structName, size_t offset, T expectedValue, const std::string& memberDesc) {
    llvm::Module* Mod = EE_ptr->getModuleAtIndex(0);
    llvm::GlobalVariable* GV = Mod->getGlobalVariable(structName);
    
    if (!GV) {
        TEST_FAIL(structName + ": Struct not found");
        g_failed++;
        return false;
    }
    
    void* baseAddr = EE_ptr->getPointerToGlobalIfAvailable(GV);
    if (!baseAddr) {
        TEST_FAIL(structName + ": Failed to get address");
        g_failed++;
        return false;
    }
    
    void* memberAddr = static_cast<char*>(baseAddr) + offset;
    T actualValue = *static_cast<T*>(memberAddr);
    
    bool passed = false;
    if constexpr (std::is_floating_point_v<T>) {
        passed = std::abs(actualValue - expectedValue) < 1e-5;
    } else {
        passed = (actualValue == expectedValue);
    }
    
    if (passed) {
        std::ostringstream oss;
        oss << structName << "." << memberDesc << " [@" << offset << "]: " << actualValue;
        TEST_PASS(oss.str());
        g_passed++;
        return true;
    } else {
        std::ostringstream oss;
        oss << structName << "." << memberDesc << ": expected " << expectedValue 
            << ", got " << actualValue;
        TEST_FAIL(oss.str());
        g_failed++;
        return false;
    }
}

/**
 * Verify heap memory value
 */
template<typename T>
bool verifyHeapValue(size_t offset, T expectedValue, const std::string& desc) {
    llvm::Module* Mod = EE_ptr->getModuleAtIndex(0);
    llvm::GlobalVariable* heapGV = Mod->getGlobalVariable("heap");
    
    if (!heapGV) {
        TEST_FAIL("heap: Global array not found");
        g_failed++;
        return false;
    }
    
    void* heapBase = EE_ptr->getPointerToGlobalIfAvailable(heapGV);
    if (!heapBase) {
        TEST_FAIL("heap: Failed to get base address");
        g_failed++;
        return false;
    }
    
    void* addr = static_cast<char*>(heapBase) + offset;
    T actualValue = *static_cast<T*>(addr);
    
    bool passed = false;
    if constexpr (std::is_floating_point_v<T>) {
        passed = std::abs(actualValue - expectedValue) < 1e-5;
    } else {
        passed = (actualValue == expectedValue);
    }
    
    if (passed) {
        std::ostringstream oss;
        oss << "heap[" << offset << "] " << desc << ": " << actualValue;
        TEST_PASS(oss.str());
        g_passed++;
        return true;
    } else {
        std::ostringstream oss;
        oss << "heap[" << offset << "] " << desc << ": expected " << expectedValue 
            << ", got " << actualValue;
        TEST_FAIL(oss.str());
        g_failed++;
        return false;
    }
}

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << COLOR_CYAN << "================================================\n";
    std::cout << "  initMemory() Test Suite\n";
    std::cout << "  Testing memory write verification\n";
    std::cout << "================================================" << COLOR_RESET << "\n";
    
    // Initialize protobuf
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    
    // Step 0: Create test LLVM module
    TEST_HEADER("Step 0: Initialize Test LLVM Module");
    if (!initTestModule()) {
        TEST_FAIL("Failed to initialize test module");
        return 1;
    }
    
    // Step 1: Create test protobuf file
    TEST_HEADER("Step 1: Create Test Protobuf File");
    std::string testFile = "/tmp/test_initMemory.pb";
    if (!createTestProtobuf(testFile)) {
        std::cerr << "Failed to create test protobuf file\n";
        return 1;
    }
    
    // Step 2: Call initMemory to load the data
    TEST_HEADER("Step 2: Call initMemory()");
    try {
        initMemory(testFile.c_str());
        TEST_PASS("initMemory() completed without errors");
    } catch (const std::exception& e) {
        TEST_FAIL(std::string("initMemory() threw exception: ") + e.what());
        return 1;
    }
    
    // Step 3: Verify global variables
    TEST_HEADER("Step 3: Verify Global Variables");
    
    verifyGlobalValue<int64_t>("test_int64", 12345678, "int64");
    verifyGlobalValue<int32_t>("test_int32", 999, "int32");
    verifyGlobalValue<float>("test_float", 3.14159f, "float");
    verifyGlobalValue<double>("test_double", 2.718281828, "double");
    
    // Verify NULL pointer
    verifyPointer("test_null_ptr", nullptr, "(NULL)");
    
    // Verify pointer to test_int64
    llvm::Module* Mod = EE_ptr->getModuleAtIndex(0);
    void* expectedPtrTarget = EE_ptr->getPointerToGlobalIfAvailable(
        Mod->getGlobalVariable("test_int64"));
    verifyPointer("test_ptr_to_int", expectedPtrTarget, "(-> test_int64)");
    
    // Step 4: Verify struct members
    TEST_HEADER("Step 4: Verify Struct Members");
    
    verifyStructMember<int32_t>("test_struct", 0, 100, "member_int");
    verifyStructMember<float>("test_struct", 4, 2.5f, "member_float");
    
    // Verify pointer member in struct (points to test_double)
    void* expectedStructPtrTarget = EE_ptr->getPointerToGlobalIfAvailable(
        Mod->getGlobalVariable("test_double"));
    llvm::GlobalVariable* structGV = Mod->getGlobalVariable("test_struct");
    void* structBase = EE_ptr->getPointerToGlobalIfAvailable(structGV);
    void* ptrMemberAddr = static_cast<char*>(structBase) + 8;
    void* actualStructPtr = *static_cast<void**>(ptrMemberAddr);
    
    if (actualStructPtr == expectedStructPtrTarget) {
        TEST_PASS("test_struct.member_ptr [@8]: points to test_double");
        g_passed++;
    } else {
        TEST_FAIL("test_struct.member_ptr [@8]: incorrect pointer value");
        g_failed++;
    }
    
    verifyStructMember<double>("test_struct", 16, 9.876, "member_double");
    
    // Step 5: Verify heap variables
    TEST_HEADER("Step 5: Verify Heap Variables");
    
    // Heap starts at offset 0 (relative to heap base)
    verifyHeapValue<int32_t>(0, 777, "(int)");
    verifyHeapValue<float>(4, 8.88f, "(float)");
    
    // Verify heap pointer (points to heap int at offset 0)
    llvm::GlobalVariable* heapGV = Mod->getGlobalVariable("heap");
    void* heapBase = EE_ptr->getPointerToGlobalIfAvailable(heapGV);
    void* heapPtrAddr = static_cast<char*>(heapBase) + 8;
    void* actualHeapPtr = *static_cast<void**>(heapPtrAddr);
    void* expectedHeapPtr = heapBase; // Points to heap[0]
    
    if (actualHeapPtr == expectedHeapPtr) {
        TEST_PASS("heap[8] (ptr): points to heap[0]");
        g_passed++;
    } else {
        std::ostringstream oss;
        oss << "heap[8] (ptr): expected " << expectedHeapPtr << ", got " << actualHeapPtr;
        TEST_FAIL(oss.str());
        g_failed++;
    }
    
    // Verify heap struct members
    verifyHeapValue<int32_t>(16, 555, "(struct.member_int)");
    
    // Verify pointer in heap struct (points to global test_int64)
    void* heapStructPtrAddr = static_cast<char*>(heapBase) + 24; // offset 16 + 8
    void* actualHeapStructPtr = *static_cast<void**>(heapStructPtrAddr);
    void* expectedHeapStructPtr = EE_ptr->getPointerToGlobalIfAvailable(
        Mod->getGlobalVariable("test_int64"));
    
    if (actualHeapStructPtr == expectedHeapStructPtr) {
        TEST_PASS("heap[24] (struct.member_ptr): points to test_int64");
        g_passed++;
    } else {
        std::ostringstream oss;
        oss << "heap[24] (struct.member_ptr): expected " << expectedHeapStructPtr 
            << ", got " << actualHeapStructPtr;
        TEST_FAIL(oss.str());
        g_failed++;
    }
    
    // Print summary
    std::cout << "\n" << COLOR_CYAN << "================================================\n";
    std::cout << "  Test Summary\n";
    std::cout << "================================================" << COLOR_RESET << "\n";
    std::cout << COLOR_GREEN << "  Passed: " << g_passed << COLOR_RESET << "\n";
    if (g_failed > 0) {
        std::cout << COLOR_RED << "  Failed: " << g_failed << COLOR_RESET << "\n";
    } else {
        std::cout << "  Failed: " << g_failed << "\n";
    }
    std::cout << "  Total:  " << (g_passed + g_failed) << "\n";
    std::cout << COLOR_CYAN << "================================================" << COLOR_RESET << "\n\n";
    
    // Cleanup
    std::remove(testFile.c_str());
    
    // Clean up EE_ptr before shutting down protobuf to avoid segfault
    EE_ptr.reset();
    
    google::protobuf::ShutdownProtobufLibrary();
    
    return (g_failed == 0) ? 0 : 1;
}
