#ifndef user_utils_HH
#define user_utils_HH

#include <string>
#include <cstdlib> 
#include "macro.hh"

namespace utils{
    // 用于获取环境变量
    std::string getEnv(const std::string &name);

    // 用于从路径中获取文件名
    std::string getFileName(const std::string& path);
    
    std::string getDirectoryPath(const std::string& fullPath);
    
}

#endif