#ifndef CONFIG_PARSE_HPP
#define CONFIG_PARSE_HPP
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include "func_worklist.hpp"

// used to represent queries item in the IR_config.yml configuration file
struct RegisterQuery {
    std::string pname;
    std::string cname;
    std::string file;
    int line = -1;
    int col = -1;
    std::string type;
};

struct StackQuery {
    std::string pname;
    std::string cname;
    std::string file;
    int line = -1;
    std::string localVarName;
    uint32_t offset;
    std::string type;
};

struct GlobalQuery {
    std::string pname;
    std::string cname;
    std::string globalVarName;
    uint32_t offset;
    std::string type;
};

struct baseIRConfig{
    std::string property_name = ""; // Property name to be verified
    std::string flightControl = ""; // Flight control name to be verified
    std::string ir = "";    // IR file to be verified
    std::vector<std::string> entrypoints; // Entry functions
    std::size_t call_depth = 0; // Depth of function calls to be processed, should be read from the configuration file
    bool isTime = false; // property whether or not time-related
};

struct IRConfig{
    baseIRConfig base;
    std::map<int, std::pair<std::string, std::string>> paramList; // Parameter list
    std::vector<RegisterQuery> registerQueries; // Register queries
    std::vector<StackQuery> stackQueries;       // Stack queries
    std::vector<GlobalQuery> globalQueries;     // Global queries
};

// struct targetNode{
//     std::string opcodeName = "";
//     int lineNum = -1;
//     int colNum = -1;
//     // std::string funcName = "";
//     std::string fileDir = "";
// };

baseIRConfig simpleParseConfig(std::string IR_config_path);

IRConfig parseConfig(std::string IR_config_path);

// std::map<int, std::set<std::string>> parseIncallsConfig(std::string incalls_config_path);

// std::map<bool, std::vector<targetNode>> parseSliceConfig(std::string IR_config_path);

std::vector<std::string> targetFuncsConfig(std::string target_funcs_config_path);

#endif