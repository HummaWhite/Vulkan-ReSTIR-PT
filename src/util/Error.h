#pragma once

#include <iostream>
#include <sstream>

#include "NamespaceDecl.h"

NAMESPACE_BEGIN(Log)

template<uint32_t NTabs, typename T>
void line(const T& msg) {
    static_assert(NTabs >= 0);
    for (uint32_t i = 0; i < NTabs; i++) {
        std::cerr << "\t";
    }
    std::cerr << "[" << msg << "]" << std::endl;
}

inline void line(const std::string& msg) {
    std::cerr << msg << std::endl;
}

inline void newLine() {
    std::cerr << std::endl;
}

inline void exit(const std::string& msg = "") {
    std::cerr << "[Error exit: " << msg << "]" << std::endl;
    std::abort();
}

inline void exception(const std::string& msg = "") {
    throw std::runtime_error(msg);
}

inline void impossiblePath() {
    exit("[Impossible path: this path is impossible to be reached, check the program]");
}

inline void check(bool cond, const std::string& errMsg = "") {
    if (!cond) {
        std::cerr << "[Check failed " << errMsg << "]" << std::endl;
        std::abort();
    }
}

NAMESPACE_END(Log)