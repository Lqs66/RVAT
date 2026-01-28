/**
 * @autor: lqs66
 * This file is used to get the forward and backward slice of specific nodes in the PDG, 
 * incidentally, we will get the slice of the functions.
 */

#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/WithColor.h>

#include "run-api.hh"

#include <chrono> 
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <filesystem>

#include "macro.hh"
#include "user_utils.hh"
#include "config_parse.hpp"
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

using namespace llvm;
using namespace std;


bool is_subDirectory(const std::string& parent_str, const std::string& child_str) {
    std::filesystem::path parent = parent_str;
    std::filesystem::path child = child_str;
    if (!std::filesystem::is_directory(parent)) {
        return false;
    }
    auto parent_abs = std::filesystem::absolute(parent);
    auto child_abs = std::filesystem::absolute(child);
    return child_abs.string().find(parent_abs.string() + std::filesystem::path::preferred_separator) == 0;
}

/**
 * @brief Filter out functions in 'boundDirs' from 'funcs'.
 */
std::set<std::string> filterFuncs(PDGCallGraph &call_g, std::set<std::string> &funcs, std::set<std::string> &boundDirs, std::string root_dir){
    std::set<std::string> ret;
    std::set<std::string> remove;
    for (auto fName : funcs){
        Node* node = call_g.getFuncNodeByName(fName);
        if (node->getNodeType() == GraphNodeType::FUNC){
            std::string func_file_dir = node->getFileDir();
            // std::cout << func_file_dir << std::endl;
            for (auto dir : boundDirs){
                if (is_subDirectory(root_dir + "/" + dir, func_file_dir)){
                    remove.insert(fName);
                    break;
                }
            }
        }
    }
    std::set_difference(funcs.begin(), funcs.end(), remove.begin(), remove.end(), std::inserter(ret, ret.begin()));
    return ret;
}

nlohmann::json getSliceCriterias(std::string property_slice_criterion_path, std::string property_name){
    std::ifstream fin(property_slice_criterion_path);
    if(!fin){
        ERROR("Failed to open property slice criterion config JSON file. " << property_slice_criterion_path);
        std::abort();
    }
    nlohmann::json property_slice_criterion;
    try{
        property_slice_criterion = nlohmann::json::parse(fin)[property_name];
        if(!property_slice_criterion.is_array()){
            ERROR("Failed to parse property slice criterion config JSON file. Expected array, got: " << property_slice_criterion.type_name());
            std::abort();
        }
    }catch(nlohmann::json::parse_error& e){
        ERROR("Failed to parse property slice criterion config JSON file. Parse error");
        std::abort();
    }
    return property_slice_criterion;
}

/**
 * Get the functions involved in the slice from the PDG.
 * 
 * @param PDG The PDG of the program.
 * @param property_slice_criterion The property slice criterion.
 * 
 *  First find all PDG nodes related to the slicing criteria, then take the union of forward and backward slices of these nodes to obtain the slice functions.
 *
 */
std::set<std::string> funcSlicing(ProgramGraph &PDG, nlohmann::json &property_slice_criterion){
    std::set<std::string> slice_funcs;
    std::set<Node*> slice_nodes;
    std::set<Node*> forward_slice, backward_slice;
    // Get all PDG nodes related to the slicing criteria
    for (auto criterion : property_slice_criterion){
        for (auto item: criterion["slicing_criterion"]){
            int line = item["line"].get<int>();
            std::string file_path = utils::getEnv("DTMC") + "/" + item["source_file"].get<std::string>();
            std::set<Node*> nodes = PDG.getNodeByAttrs(line, file_path);
            slice_nodes.insert(nodes.begin(), nodes.end());
        }
    }
    // Get forward and backward slices of all slice nodes
    for (auto node : slice_nodes){
        std::set<Node*> forward_nodes = PDG.forward_slice(*node);
        std::set<Node*> backward_nodes = PDG.backward_slice(*node);
        forward_slice.insert(forward_nodes.begin(), forward_nodes.end());
        backward_slice.insert(backward_nodes.begin(), backward_nodes.end());
    }
    // Get slice functions from forward and backward slices
    for (auto node : forward_slice){
        if (node->getFunc() != nullptr){
            slice_funcs.insert(node->getFunc()->getName().str());
        }
    }
    for (auto node : backward_slice){
        if (node->getFunc() != nullptr){
            slice_funcs.insert(node->getFunc()->getName().str());
        }
    }
    return slice_funcs;
}

std::set<Node*> getCallGraphFuncsFromSlice(PDGCallGraph &call_g, std::string &start_func_name, std::set<std::string> &slice_funcs, std::set<std::string> &boundDirs, std::string root_dir){
    std::set<Node*> result;
    slice_funcs = filterFuncs(call_g, slice_funcs, boundDirs, root_dir);
    std::set<Node*> cg_slice = call_g.getSubCallGraph(start_func_name, slice_funcs);
    for (auto node : cg_slice){
        if (auto value = node->getValue()){
            if (auto func = dyn_cast<Function>(value)){
                result.insert(node);
            }
        }
    }
    return result;
}

// void autoGenAnalysisDepth(std::string entryName, std::set<Node*> &funcs, std::string yaml_file){
//     Node* start = nullptr;
//     for (auto node : funcs){
//         if (auto value = node->getValue()){
//             if (auto func = dyn_cast<Function>(value)){
//                 if (func->getName().str() == entryName){
//                     start = node;
//                     break;
//                 }
//             }
//         }
//     }
//     std::queue<Node*> queue;
//     std::set<Node*> visited;
//     std::map<Node*, int> nodeDepth;
//     queue.push(start);
//     visited.insert(start);
//     nodeDepth[start] = 0;
//     while (!queue.empty()){
//         Node* current = queue.front();
//         queue.pop();
//         for (auto edge : current->getOutEdgeSet()){
//             Node* dst = edge->getDstNode();
//             if (funcs.count(dst)){
//                 if (visited.count(dst) == 0){
//                     queue.push(dst);
//                     visited.insert(dst);
//                     nodeDepth[dst] = nodeDepth[current] + 1;
//                 }
//             }
//         }
//     }
//     int call_depth = 0;
//     for (auto node : funcs){
//         call_depth = std::max(call_depth, nodeDepth[node]);
//     }

//     YAML::Node config = YAML::LoadFile(yaml_file);
//     config["call_depth"] = call_depth;
//     std::ofstream fout(yaml_file);
//     fout << config;
//     fout.close();
//     std::cout << "Call depth is set to " << call_depth << "." << std::endl;
// }

void dumpSubCallGraphFuncs(std::set<Node*> &funcs, std::string dot_file){
    std::set<std::pair<std::string, std::string>> dotEdges;
    for (auto node : funcs){
        for (auto edge : node->getOutEdgeSet()){
            Node* dst = edge->getDstNode();
            if (funcs.count(dst)){
                std::string srcFuncName = "";
                if (auto value = node->getValue()){
                    if (auto func = dyn_cast<Function>(value)){
                        srcFuncName = func->getName().str();
                    }
                }
                std::string dstFuncName = "";
                if (auto value = dst->getValue()){
                    if (auto func = dyn_cast<Function>(value)){
                        dstFuncName = func->getName().str();
                    }
                }
                if (srcFuncName != "" && dstFuncName != ""){
                    dotEdges.insert(std::make_pair(srcFuncName, dstFuncName));
                }
            }
        }
    }
    std::ofstream out(dot_file);
    out << "digraph G {\n";
    for (auto pair : dotEdges){
        out << pair.first << " -> " << pair.second << ";\n";
    }
    out << "}\n";
    out.close();
}

/*
functions:
    - func1
      func2
      func3
      func4
      ...
 */
void dumpTargetFuncsToYAML(std::set<Node*> &funcs, std::string yaml_file){
    std::ofstream out(yaml_file);
    out << "functions:\n";
    for (auto node : funcs){
        if (auto value = node->getValue()){
            if (auto func = dyn_cast<Function>(value)){
                std::string curr_func = func->getName().str();
                out << "  - " << curr_func << "\n";            
            }
        }
    }
    out.close();
}

int main(int argc, char **argv)
{
    auto start = std::chrono::high_resolution_clock::now();
    // Initialize LLVM
    InitLLVM X(argc, argv);

    cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input IR file>"), cl::init("-"));
    cl::opt<bool> DumpFlag("dump-pdg", cl::desc("Dump the pdg"), cl::init(false));
    cl::opt<std::string> outputPath("o", cl::desc("Specify the output path"), llvm::cl::value_desc("path"));

    cl::ParseCommandLineOptions(argc, argv, "LLVM IR File Reader\n");

    std::string DTMC = utils::getEnv("DTMC");
    if (DTMC == ""){
        outs() << "env variable DTMC is not set!" << "\n";
        return 1;
    }

    // check output path exists, if not, create it
    if (outputPath != ""){
        std::filesystem::path p(outputPath.getValue());
        if (!std::filesystem::exists(p)){
            std::filesystem::create_directories(p);
        }
    }

    LLVMContext ctx;
    SMDiagnostic Err;
    std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, ctx);
    if (!M) {
        outs() << InputFilename << " Not Exist!" << "\n";
        return 1;
    }

    // std::map<int, std::set<std::string>> incallsMap = {};
    // if (ConfigFilename != ""){
    //     incallsMap = parseIncallsConfig(ConfigFilename);
    // }

    // runPDGPass(*M, incallsMap, DumpFlag);
    runPDGPass(*M, DumpFlag);
    ProgramGraph &pdg = ProgramGraph::getInstance();

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    std::cout << std::fixed << std::setprecision(2)
    << "building PDG takes: " <<  duration.count()/1000.0 << " seconds." << std::endl;

    PDGCallGraph &call_g = PDGCallGraph::getInstance();

    std::string IR_config = DTMC + "/configs/IR_config.yml";
    baseIRConfig base_config = simpleParseConfig(IR_config);
    std::string entryFuncName = base_config.entrypoints[0];
    std::string property = base_config.property_name;

    std::set<std::string> boundDirs = {
        "flight-control/arducopter-4.4/libraries/AP_Logger",
        "flight-control/arducopter-4.4/libraries/AP_Filesystem",
        "flight-control/arducopter-4.4/libraries/AP_ROMFS",
        "flight-control/arducopter-4.4/libraries/AP_Notify",
        "flight-control/arducopter-4.4/libraries/AP_InternalError",
        "flight-control/arducopter-4.4/libraries/GCS_MAVLink",
        "flight-control/arducopter-4.4/build/sitl/libraries/GCS_MAVLink",
        "flight-control/arducopter-4.4/libraries/AP_OpenDroneID",
        "flight-control/arducopter-4.4/libraries/AP_Scripting",
        "flight-control/arducopter-4.4/libraries/AP_CSVReader",
        "flight-control/arducopter-4.4/libraries/AP_FlashIface",
        "flight-control/arducopter-4.4/libraries/AP_FlashStorage",
        "flight-control/arducopter-4.4/libraries/AP_SerialLED",
        "flight-control/arducopter-4.4/libraries/AP_VideoTX",
        "flight-control/arducopter-4.4/libraries/AP_Camera"
        // "flight-control/arducopter-4.4/libraries/AP_UAVCAN",
        // "flight-control/arducopter-4.4/modules",
        // "flight-control/arducopter-4.4/build/sitl/modules",
        // "flight-control/arducopter-4.4/libraries/AP_Torqeedo"
    };
    // start generating slice
    start = std::chrono::high_resolution_clock::now(); 

    nlohmann::json property_slice_criterion_map = getSliceCriterias(DTMC + "/configs/property_slice_criterion.json", property);
    
    std::set<std::string> slice_funcs = funcSlicing(pdg, property_slice_criterion_map);
        
    std::set<Node*> result_functions = getCallGraphFuncsFromSlice(call_g, entryFuncName, slice_funcs, boundDirs, DTMC);    
    INFO("Result Functions size: " << result_functions.size());
    // dumpSubCallGraphFuncs(result_functions, outputPath + "/" + base_config.property_name + "_Slice_CG.dot");
    dumpTargetFuncsToYAML(result_functions, outputPath + "/" + base_config.property_name + "_Slice_FS.yml");
    stop = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    INFO(std::fixed << std::setprecision(2) << "computing slice takes: " <<  duration.count()/1000.0 << " seconds.");
}