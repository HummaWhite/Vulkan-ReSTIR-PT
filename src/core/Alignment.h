#pragma once

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

namespace Std140 {
    template<typename T>
    constexpr int alignmentOf() {
        static_assert(std::is_class_v<T>, "zvk::Alignment: not a class or scalar less than 4 bytes");
        return ((sizeof(T) + 15) / 16) * 16;
    }

    template<> constexpr int alignmentOf<int32_t>()     { return 4; }
    template<> constexpr int alignmentOf<uint32_t>()    { return 4; }
    template<> constexpr int alignmentOf<float>()       { return 4; }
    template<> constexpr int alignmentOf<glm::vec2>()   { return 8; }
    template<> constexpr int alignmentOf<glm::ivec2>()  { return 8; }
    template<> constexpr int alignmentOf<glm::uvec2>()  { return 8; }
    template<> constexpr int alignmentOf<glm::vec3>()   { return 16; }
    template<> constexpr int alignmentOf<glm::ivec3>()  { return 16; }
    template<> constexpr int alignmentOf<glm::uvec3>()  { return 16; }
    template<> constexpr int alignmentOf<glm::vec4>()   { return 16; }
    template<> constexpr int alignmentOf<glm::ivec4>()  { return 16; }
    template<> constexpr int alignmentOf<glm::uvec4>()  { return 16; }
    template<> constexpr int alignmentOf<glm::mat3>()   { return 16; }
    template<> constexpr int alignmentOf<glm::mat4>()   { return 16; }
    template<> constexpr int alignmentOf<glm::mat3x4>() { return 16; }
    template<> constexpr int alignmentOf<glm::mat4x3>() { return 16; }
}

NAMESPACE_END(zvk)

#define std140(type, name) alignas(zvk::Std140::alignmentOf<type>()) type name