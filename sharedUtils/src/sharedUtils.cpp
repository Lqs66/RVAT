#include "config_parse.hpp"
#include "func_worklist.hpp"
#include "user_utils.hh"
#include "llvm/IR/Intrinsics.h"

baseIRConfig simpleParseConfig(std::string IR_config_path){
    baseIRConfig baseConfig;
    std::ifstream fin(IR_config_path);
    if(!fin){
        ERROR("Failed to open Model config YAML file. Config File Path: " << IR_config_path);
        std::abort();
    }
    YAML::Node configNode = YAML::Load(fin);
    if(configNode["property"].IsDefined()){
        baseConfig.property_name = configNode["property"].as<std::string>();
    }else{
        ERROR("property_name is not specified in the config file. Config File Path: " << IR_config_path);
        std::abort();
    }
    if(configNode["flightControl"].IsDefined()){
        baseConfig.flightControl = configNode["flightControl"].as<std::string>();
    }else{
        ERROR("flightControl is not specified in the config file. Config File Path: " << IR_config_path);
        std::abort();
    }
    if(configNode["LLVM_IR"].IsDefined()){
        baseConfig.ir = configNode["LLVM_IR"].as<std::string>();
    }else{
        ERROR("LLVM_IR is not specified in the config file. Config File Path: " << IR_config_path);
        std::abort();
    }
    if(configNode["call_depth"].IsDefined()){
        baseConfig.call_depth = configNode["call_depth"].as<std::size_t>();
    }else{
        WARNING("call_depth is not specified in the config file. Config File Path: " << IR_config_path << ". Use default value 0.");
    }
    if(configNode["entrypoints"].IsDefined()){
        for (const auto& entry : configNode["entrypoints"]) {
            baseConfig.entrypoints.push_back(entry.as<std::string>());
        }
    }else{
        ERROR("Entrypoints is not specified in the config file. Config File Path: " << IR_config_path);
        std::abort();
    }
    if(configNode["isTime"].IsDefined()){
        baseConfig.isTime = configNode["isTime"].as<bool>();
    }else{
        ERROR("isTime is not specified in the config file. Config File Path: " << IR_config_path << ". Use default value false.");
        std::abort();
    }
    return baseConfig;
}

/**
 * @brief Parse the configuration file IR_config.yml
 */
IRConfig parseConfig(std::string IR_config_path){
    IRConfig config;
    baseIRConfig baseConfig = simpleParseConfig(IR_config_path);
    config.base = baseConfig;
    std::ifstream fin(IR_config_path);
    if(!fin){
        ERROR("Failed to open YAML file. " << IR_config_path);
        std::abort();
    }
    YAML::Node configNode = YAML::Load(fin);
    if(configNode["params"].IsDefined()){
        for (const auto& param : configNode["params"]){
            int id = param["id"].as<int>();
            std::string type = param["type"].as<std::string>();
            std::string value = param["value"].as<std::string>();
            config.paramList.emplace(id, std::make_pair(type, value));
        }
    }else{
        ERROR("params is not specified in the config file. Config File Path: " << IR_config_path);
        std::abort();
    }
    if (configNode["queries"].IsDefined()){
        for (const auto& query : configNode["queries"]){
            std::string scope = query["scope"].as<std::string>();
            if (scope == "register"){
                RegisterQuery lq;
                lq.pname = query["pname"].as<std::string>();
                lq.cname = query["cname"].as<std::string>();
                lq.file = query["file"].as<std::string>();
                lq.line = query["line"].as<int>();
                lq.col = query["column"].as<int>();
                lq.type = query["type"].as<std::string>();
                config.registerQueries.push_back(lq);
            }else if (scope == "stack"){
                StackQuery sq;
                sq.pname = query["pname"].as<std::string>();
                sq.cname = query["cname"].as<std::string>();
                sq.file = query["file"].as<std::string>();
                sq.line = query["line"].as<int>();
                std::string field = query["field"].as<std::string>();
                // field format: localVarName + offset, such as "rate_ef_level_cd + 0".
                // We need to split it.
                size_t plusPos = field.find('+');
                if (plusPos != std::string::npos){
                    sq.localVarName = field.substr(0, plusPos - 1);
                    std::string offsetStr = field.substr(plusPos + 1);
                    sq.offset = std::stoul(offsetStr);
                }else{
                    ERROR("Invalid field format in global query: " << field << ". Config File Path: " << IR_config_path);
                    std::abort();
                }
                sq.type = query["type"].as<std::string>();
                config.stackQueries.push_back(sq);
            }else if (scope == "global"){
                GlobalQuery gq;
                gq.pname = query["pname"].as<std::string>();
                gq.cname = query["cname"].as<std::string>();
                std::string field = query["field"].as<std::string>();
                // field format: globalVarName + offset, such as "heap + 32".
                // We need to split it.
                size_t plusPos = field.find('+');
                if (plusPos != std::string::npos){
                    gq.globalVarName = field.substr(0, plusPos - 1);
                    std::string offsetStr = field.substr(plusPos + 1);
                    gq.offset = std::stoul(offsetStr);
                }else{
                    ERROR("Invalid field format in global query: " << field << ". Config File Path: " << IR_config_path);
                    std::abort();
                }
                gq.type = query["type"].as<std::string>();
                config.globalQueries.push_back(gq);
            }else{
                ERROR("Unknown query scope: " << scope << ". Config File Path: " << IR_config_path);
                std::abort();
            }
        }
    }else{
        ERROR("queries is not specified in the config file. Config File Path: " << IR_config_path);
        std::abort();
    }
    return config;
}

// std::map<int, std::set<std::string>> parseIncallsConfig(std::string incalls_config_path){
//     std::map<int, std::set<std::string>> incallsMap;
//     std::ifstream fin(incalls_config_path);
//     if(!fin){
//         ERROR("Failed to open incalls config YAML file. " << incalls_config_path);
//         std::abort();
//     }
//     YAML::Node config = YAML::Load(fin);
//     if (config["indirectCalls"].IsDefined()){
//         for (const auto& incall : config["indirectCalls"]){
//             int ID = incall["inCallID"].as<int>();
//             std::set<std::string> callees;
//             for (const auto& callee : incall["targets"]){
//                 callees.insert(callee.as<std::string>());
//             }
//             incallsMap.emplace(ID, callees);
//         }
//     }else{
//         ERROR("indirectCalls is not specified in the incalls config file.");
//         std::abort();
//     }
//     return incallsMap;
// }

/**
 *  parse slice information part in IR_config.yml 
    forward: 
     - opcode: load
       line: 368
       loc: 37
       file: /home/lqs66/Desktop/modelCheckingFlightControl/flight-control/arducopter-4.4/ArduCopter/mode_rtl.cpp
    backward:
     - opcode: store
       line: 368
       loc: 37
       file: /home/lqs66/Desktop/modelCheckingFlightControl/flight-control/arducopter-4.4/ArduCopter/mode_rtl.cpp
 */
// std::map<bool, std::vector<targetNode>> parseSliceConfig(std::string IR_config_path){
//     std::map<bool, std::vector<targetNode>> sliceConfigs;
//     std::ifstream fin(IR_config_path);
//     if(!fin){
//         ERROR("Failed to open slice config YAML file. " << IR_config_path);
//         std::abort();
//     }
//     YAML::Node config = YAML::Load(fin);
//     if(config["property"].IsDefined()){
//         INFO("Parsing slice config for: " << config["property"].as<std::string>());
//     }else{
//         ERROR("property_name is not specified in the config file. Config File Path: " << IR_config_path);
//         std::abort();
//     }
    
//     if (config["forward"].IsDefined()){
//         std::vector<targetNode> forward_targetNodes;
//         for (const auto& target : config["forward"]){
//             targetNode node;
//             node.opcodeName = target["opcode"].as<std::string>();
//             node.lineNum = target["line"].as<int>();
//             node.colNum = target["loc"].as<int>();
//             node.fileDir = target["file"].as<std::string>();
//             forward_targetNodes.push_back(node);
//         }
//         sliceConfigs.emplace(true, forward_targetNodes);
//     }else{
//         // When forward does not exist, return an empty vector
//         sliceConfigs.emplace(true, std::vector<targetNode>());
//     }
    
//     if (config["backward"].IsDefined()){
//         std::vector<targetNode> backward_targetNodes;
//         for (const auto& target : config["backward"]){
//             targetNode node;
//             node.opcodeName = target["opcode"].as<std::string>();
//             node.lineNum = target["line"].as<int>();
//             node.colNum = target["loc"].as<int>();
//             node.fileDir = target["file"].as<std::string>();
//             backward_targetNodes.push_back(node);
//         }
//         sliceConfigs.emplace(false, backward_targetNodes);
//     }else{
//         // When backward does not exist, return an empty vector
//         sliceConfigs.emplace(false, std::vector<targetNode>());
//     }
    
//     return sliceConfigs;
// }

std::vector<std::string> targetFuncsConfig(std::string target_funcs_config_path){
    std::vector<std::string> targetFuncs;
    std::ifstream fin(target_funcs_config_path);
    if(!fin){
        ERROR("Failed to open target funcs config YAML file. " << target_funcs_config_path);
        std::abort();
    }
    YAML::Node config = YAML::Load(fin);
    if (config["functions"].IsDefined()){
        for (const auto& func : config["functions"]){
            targetFuncs.push_back(func.as<std::string>());
        }
    }else{
        ERROR("functions is not specified in the target funcs config file.");
        std::abort();
    }
    return targetFuncs;
}

namespace utils{
    std::vector<std::string> dmaFuncs = {"malloc", "calloc", "realloc", "_Znwm", "_Znam"};

    std::vector<llvm::Function*> convertFunctionNameToFunction(llvm::Module& module, std::vector<std::string> functionNames){
        std::vector<llvm::Function*> functions;
        for(std::string funcName : functionNames){
            llvm::Function* func = module.getFunction(funcName);
            if(func == nullptr){
                ERROR("Function: " << funcName << " not found.");
                std::abort();
            }
            functions.push_back(func);
        }
        return functions;
    }

    std::string getEnv(const std::string &name){
        const char *val = std::getenv(name.c_str());
        if(val == nullptr){
            ERROR("Environment variable: " << name << " not found.");
            return "";
        }
        return std::string(val);
    }

    std::string getFileName(const std::string& path) {
        // Find the position of the last slash or backslash
        size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos) {
            // If no slash is found, return the entire path
            return path;
        }
        // Return the filename part
        return path.substr(pos + 1);
    }

    std::string getDirectoryPath(const std::string& fullPath) {
        size_t pos = fullPath.find_last_of("/\\");
        if (pos != std::string::npos) {
            return fullPath.substr(0, pos);
        }
        return "";
    }

}
