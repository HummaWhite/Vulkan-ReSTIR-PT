file(GLOB_RECURSE core_sources
	"${CMAKE_CURRENT_SOURCE_DIR}/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

#file(GLOB IMAGES
#    ${CMAKE_CURRENT_SOURCE_DIR}/images/*.jpg
#    ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png
#    ${CMAKE_CURRENT_SOURCE_DIR}/images/*.bmp
#)

#foreach(IMAGE ${IMAGES})
#    get_filename_component(fname ${IMAGE} NAME)
#    configure_file(${IMAGE} ${CMAKE_CURRENT_BINARY_DIR}/images/${fname} COPYONLY)
#endforeach()

file(GLOB_RECURSE SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/*.geom
    ${CMAKE_CURRENT_SOURCE_DIR}/*.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.tese
    ${CMAKE_CURRENT_SOURCE_DIR}/*.tesc
	${CMAKE_CURRENT_SOURCE_DIR}/*.rgen
	${CMAKE_CURRENT_SOURCE_DIR}/*.rmiss
	${CMAKE_CURRENT_SOURCE_DIR}/*.rchit
)

file(GLOB_RECURSE SHADER_INCLUDES
	${CMAKE_CURRENT_SOURCE_DIR}/*.glsl
)

source_group("Shaders" FILES ${SHADER_SOURCES})

if(MSVC_IDE)
	# Macro to preserve source files hierarchy in the IDE
	macro(GroupSources curdir)
		file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)

		foreach(child ${children})
			if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
				GroupSources(${curdir}/${child})
			else()
				string(REPLACE "/" "\\" groupname ${curdir})
				string(REPLACE "src" "Sources" groupname ${groupname})
				source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
			endif()
		endforeach()
	endmacro()

	GroupSources(${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(WIN32)
    add_executable(Vulkan_ReSTIR_PT ${core_sources} ${SHADER_SOURCES} ${SHADER_INCLUDES})
    target_link_libraries(Vulkan_ReSTIR_PT ${WINLIBS})
else(WIN32)
    add_executable(Vulkan_ReSTIR_PT ${core_sources})
    target_link_libraries(Vulkan_ReSTIR_PT ${CMAKE_THREAD_LIBS_INIT})
endif(WIN32)

foreach(SHADER_SOURCE ${SHADER_SOURCES})
    set(SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

    if(WIN32)
        get_filename_component(fname ${SHADER_SOURCE} NAME)
        add_custom_target(${fname}.spv
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR} && 
            $ENV{VK_SDK_PATH}/Bin/glslc.exe ${SHADER_SOURCE} -o ${SHADER_DIR}/${fname}.spv --target-env=vulkan1.2
            SOURCES ${SHADER_SOURCE}
        )
        ExternalTarget("Shaders" ${fname}.spv)
        add_dependencies(Vulkan_ReSTIR_PT ${fname}.spv)
    endif(WIN32)

    # TODO: Build shaders on not windows
endforeach()

target_link_libraries(Vulkan_ReSTIR_PT Vulkan::Vulkan glfw stb assimp pugixml)

target_include_directories(Vulkan_ReSTIR_PT
	PRIVATE
  		${CMAKE_CURRENT_SOURCE_DIR}
)

InternalTarget("" Vulkan_ReSTIR_PT)
