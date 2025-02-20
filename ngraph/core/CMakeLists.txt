# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

add_definitions(-DIN_NGRAPH_LIBRARY)

file(GLOB_RECURSE LIBRARY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
                              ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp)
file(GLOB_RECURSE PUBLIC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

add_subdirectory(builder)
add_subdirectory(reference)
add_subdirectory(shape_inference)

# WA for Tensor implementation via ie::Blob::Ptr
set(IE_SRC_ROOT "${IE_MAIN_SOURCE_DIR}/src/inference_engine/src")
set(IE_SHARED_SRCS
    "${IE_SRC_ROOT}/system_allocator.cpp"
    "${IE_SRC_ROOT}/blob_factory.cpp"
    "${IE_SRC_ROOT}/ie_blob_common.cpp"
    "${IE_SRC_ROOT}/system_allocator.cpp"
    "${IE_SRC_ROOT}/ie_layouts.cpp")
set(MIXED_SRC ${IE_SHARED_SRCS}
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/allocator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/ov_tensor.cpp")

set_property(SOURCE ${MIXED_SRC}
    APPEND PROPERTY INCLUDE_DIRECTORIES
        ${IE_SRC_ROOT}
        $<TARGET_PROPERTY:inference_engine,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:inference_engine_plugin_api,INTERFACE_INCLUDE_DIRECTORIES>)

set_source_files_properties(${MIXED_SRC}
    PROPERTIES COMPILE_DEFINITIONS IMPLEMENT_INFERENCE_ENGINE_API)

# Create named folders for the sources within the .vcproj
# Empty name lists them directly under the .vcproj

source_group("src" FILES ${LIBRARY_SRC})
source_group("include" FILES ${PUBLIC_HEADERS})

# Create static or shared library depending on BUILD_SHARED_LIBS
add_library(ngraph ${LIBRARY_SRC} ${PUBLIC_HEADERS} ${IE_SHARED_SRCS})


if(COMMAND ie_faster_build)
    ie_faster_build(ngraph
        UNITY
        PCH PRIVATE "src/precomp.hpp"
    )
endif()

if(COMMAND ie_add_api_validator_post_build_step)
    ie_add_api_validator_post_build_step(TARGET ngraph)
endif()

if(COMMAND ie_add_vs_version_file)
    ie_add_vs_version_file(NAME ngraph
                           FILEDESCRIPTION "nGraph library")
endif()

addVersionDefines(src/version.cpp CI_BUILD_NUMBER)

target_link_libraries(ngraph PRIVATE ngraph::builder ngraph::reference openvino::util ov_shape_inference)

ie_mark_target_as_cc(ngraph)

ov_ncc_naming_style(FOR_TARGET ngraph
                    INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_clang_format_target(ngraph_clang FOR_TARGETS ngraph)

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(ngraph PUBLIC NGRAPH_STATIC_LIBRARY)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # ngraph is linked against ngraph_builders, ngraph_reference static libraries
    # which include ngraph headers with dllimport attribute. Linker complains about it
    # but no way to fix this: linking with no attribute defaults to dllexport and we have
    # multiple defitions for ngraph symbols.
    #
    # The possible way is to use object libraries for ngraph_builders, ngraph_reference
    # but it's not convinient since these libraries are exported from build tree
    # and it's better to use them as static libraries in 3rd party projects
    set_target_properties(ngraph PROPERTIES LINK_FLAGS "/IGNORE:4217,4286")
endif()

# some sources are located in ngraph, while headers are in inference_engine_transformations
file(GLOB_RECURSE smart_reshape_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/pass/smart_reshape/*.cpp)
file(GLOB_RECURSE rt_info_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/pass/rt_info/*.cpp)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/pass/convert_precision.cpp"
                            "${CMAKE_CURRENT_SOURCE_DIR}/src/pass/convert_fp32_to_fp16.cpp"
                            "${CMAKE_CURRENT_SOURCE_DIR}/src/pass/init_node_info.cpp"
                            "${CMAKE_CURRENT_SOURCE_DIR}/src/op/type_relaxed.cpp"
                            "${CMAKE_CURRENT_SOURCE_DIR}/src/function.cpp" # for SmartReshape
                            ${smart_reshape_srcs} ${rt_info_srcs}
        PROPERTIES INCLUDE_DIRECTORIES $<TARGET_PROPERTY:inference_engine_transformations,INTERFACE_INCLUDE_DIRECTORIES>)

# Defines macro in C++ to load backend plugin
target_include_directories(ngraph PUBLIC $<BUILD_INTERFACE:${NGRAPH_INCLUDE_PATH}>
                                  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(ngraph::ngraph ALIAS ngraph)
add_library(openvino::core ALIAS ngraph)

target_link_libraries(ngraph PRIVATE ${CMAKE_DL_LIBS})

#-----------------------------------------------------------------------------------------------
# Export for build tree
#-----------------------------------------------------------------------------------------------

set_target_properties(ngraph PROPERTIES EXPORT_NAME core)
export(TARGETS ngraph NAMESPACE openvino::
       APPEND FILE "${CMAKE_BINARY_DIR}/OpenVINOTargets.cmake")

#-----------------------------------------------------------------------------------------------
# Installation logic...
#-----------------------------------------------------------------------------------------------

install(TARGETS ngraph EXPORT OpenVINOTargets
        RUNTIME DESTINATION ${IE_CPACK_RUNTIME_PATH} COMPONENT ngraph
        ARCHIVE DESTINATION ${IE_CPACK_ARCHIVE_PATH} COMPONENT ngraph
        LIBRARY DESTINATION ${IE_CPACK_LIBRARY_PATH} COMPONENT ngraph
        INCLUDES DESTINATION runtime/include)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION "runtime/include"
        COMPONENT ngraph_dev
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "*.h")

configure_package_config_file(${OpenVINO_SOURCE_DIR}/cmake/templates/ngraphConfig.cmake.in
    ${ngraph_BINARY_DIR}/ngraphConfig.cmake
    INSTALL_DESTINATION cmake)

write_basic_package_version_file(${ngraph_BINARY_DIR}/ngraphConfigVersion.cmake
    VERSION ${IE_VERSION_MAJOR}.${IE_VERSION_MINOR}.${IE_VERSION_PATCH}
    COMPATIBILITY SameMajorVersion)

install(FILES ${ngraph_BINARY_DIR}/ngraphConfig.cmake
              ${ngraph_BINARY_DIR}/ngraphConfigVersion.cmake
        DESTINATION "runtime/cmake"
        COMPONENT ngraph_dev)
