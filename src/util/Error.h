#pragma once

#include <iostream>
#include <sstream>

#include "NamespaceDecl.h"

NAMESPACE_BEGIN(Log)

template<int NTabs, typename T>
void bracketLine(const T& msg) {
    static_assert(NTabs >= 0);
    for (int i = 0; i < NTabs; i++) {
        std::cerr << "\t";
    }
    std::cerr << "[" << msg << "]" << std::endl;
}

static void line(const std::string& msg) {
    std::cerr << msg << std::endl;
}

static void exit(const std::string& msg = "") {
    std::cerr << "[Error exit: " << msg << "]" << std::endl;
    std::abort();
}

static void exception(const std::string& msg = "") {
    throw std::runtime_error(msg);
}

static void impossiblePath() {
    exit("[Impossible path: this path is impossible to be reached, check the program]");
}

static void check(bool cond, const std::string& errMsg = "") {
    if (!cond) {
        std::cerr << "[Check failed " << errMsg << "]" << std::endl;
        std::abort();
    }
}

NAMESPACE_END(Log)