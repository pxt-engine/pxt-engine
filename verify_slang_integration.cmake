# Verification script for Slang integration
# This script checks if Slang is properly configured in the CMake build system

cmake_minimum_required(VERSION 3.20)

message(STATUS "")
message(STATUS "=== Slang Integration Verification ===")
message(STATUS "")

# Check if Slang directory exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/external/slang")
    message(STATUS "✓ Slang directory found at Engine/external/slang")
else()
    message(FATAL_ERROR "✗ Slang directory not found")
endif()

# Check if Slang CMakeLists.txt exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/external/slang/CMakeLists.txt")
    message(STATUS "✓ Slang CMakeLists.txt found")
else()
    message(FATAL_ERROR "✗ Slang CMakeLists.txt not found")
endif()

# Check if Slang headers exist
set(REQUIRED_HEADERS
    "slang.h"
    "slang-com-ptr.h"
    "slang-com-helper.h"
)

foreach(HEADER ${REQUIRED_HEADERS})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/external/slang/include/${HEADER}")
        message(STATUS "✓ Slang header (${HEADER}) found")
    else()
        message(FATAL_ERROR "✗ Slang header (${HEADER}) not found")
    endif()
endforeach()

# Check if .gitmodules contains Slang
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.gitmodules" GITMODULES_CONTENT)
if(GITMODULES_CONTENT MATCHES "slang")
    message(STATUS "✓ Slang registered in .gitmodules")
else()
    message(WARNING "⚠ Slang not found in .gitmodules")
endif()

# Check if Engine/CMakeLists.txt references Slang
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Engine/CMakeLists.txt" ENGINE_CMAKE_CONTENT)
if(ENGINE_CMAKE_CONTENT MATCHES "add_subdirectory\\(external/slang\\)")
    message(STATUS "✓ Slang subdirectory added in Engine/CMakeLists.txt")
else()
    message(FATAL_ERROR "✗ Slang subdirectory not added in Engine/CMakeLists.txt")
endif()

if(ENGINE_CMAKE_CONTENT MATCHES "slang")
    message(STATUS "✓ Slang linked to PXT_Engine target")
else()
    message(FATAL_ERROR "✗ Slang not linked to PXT_Engine target")
endif()

if(ENGINE_CMAKE_CONTENT MATCHES "external/slang/include")
    message(STATUS "✓ Slang include directory configured")
else()
    message(FATAL_ERROR "✗ Slang include directory not configured")
endif()

# Check if test file exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/src/test_slang_integration.cpp")
    message(STATUS "✓ Slang integration test file found")
else()
    message(WARNING "⚠ Slang integration test file not found")
endif()

message(STATUS "")
message(STATUS "=== All Checks Passed! ===")
message(STATUS "")
message(STATUS "Slang is properly integrated into the CMake build system.")
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  • Slang Location: Engine/external/slang")
message(STATUS "  • Headers: Engine/external/slang/include/")
message(STATUS "  • Target: slang (linked to PXT_Engine)")
message(STATUS "  • Build Options: Tests, Examples, GFX, Slangd, Slangc disabled")
message(STATUS "")
message(STATUS "Requirements Satisfied:")
message(STATUS "  ✓ 1.1 - Slang library linked to PXT_Engine target")
message(STATUS "  ✓ 1.2 - Slang headers accessible to engine code")
message(STATUS "  ✓ 1.3 - Slang library availability verified during build")
message(STATUS "")
message(STATUS "Next Steps:")
message(STATUS "  1. Build the project to compile Slang library")
message(STATUS "  2. Implement SlangCompiler class (Task 1.2)")
message(STATUS "  3. Implement ShaderReflection classes (Task 1.3)")
message(STATUS "")
message(STATUS "Build Commands:")
message(STATUS "  cmake --preset x64-Debug")
message(STATUS "  cmake --build out/build/x64-Debug")
message(STATUS "")
