#pragma once

#include "NamespaceDecl.h"

NAMESPACE_BEGIN(util)

inline uint32_t ceilDiv(uint32_t x, uint32_t y) {
    return (x + y - 1) / y;
}

inline uint32_t align(uint32_t size, uint32_t alignment) {
    return ceilDiv(size, alignment) * alignment;
}

NAMESPACE_END(util)
