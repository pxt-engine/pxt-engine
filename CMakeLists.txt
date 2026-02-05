cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME "PXT")
set(VERSION "0.0.1")

project(${PROJECT_NAME} VERSION ${VERSION} LANGUAGES C CXX)

# Global C++ settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out")
set(SHADER_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/shaders")

# Shader directory
set(SHADER_DIRECTORY "${CMAKE_SOURCE_DIR}/assets/shaders")

# CMake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Dependencies
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

# modules
include(CompileShaders)

# Subprojects
add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Application)

compile_shaders(${PROJECT_NAME} ${SHADER_DIRECTORY} ${SHADER_OUTPUT_DIRECTORY})

# MSVC specific settings
if (MSVC)
    # If compiling with MSVC, we ignore warning 4099 which is about debug information for PDB files.
    # This is because the shaderc library does not provide pdb files, and MSVC will complain about it.
    target_link_options(${PROJECT_NAME} PRIVATE /IGNORE:4099)

    # Enable static analysis
    target_compile_options(${PROJECT_NAME} PRIVATE /analyze)
endif()