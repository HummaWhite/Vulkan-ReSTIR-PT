set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
add_subdirectory(GLFW)

file(GLOB stb_sources
	"${CMAKE_CURRENT_SOURCE_DIR}/stb.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/stb/*.h")

add_library(stb ${stb_sources})

target_include_directories(stb PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/stb")

file(GLOB imgui_sources
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_vulkan.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_vulkan.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp")

add_library(imgui ${imgui_sources})

target_include_directories(imgui PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends")

add_subdirectory(assimp)
add_subdirectory(pugixml)
