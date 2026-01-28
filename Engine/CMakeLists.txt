project(PXT_Engine LANGUAGES CXX)

# Create the static library target for the engine
add_library(PXT_Engine STATIC)

# Specify the source files for the library
file(GLOB_RECURSE ENGINE_SOURCES
    src/*.cpp
    src/*.hpp
)

# Add the source files to the target
target_sources(PXT_Engine PRIVATE ${ENGINE_SOURCES})

target_include_directories(PXT_Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/entt/single_include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp/include
)

# Third-party external libraries with CMake support
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/tinyobjloader)
add_subdirectory(external/entt)
add_subdirectory(external/spdlog)
add_subdirectory(external/yaml-cpp)

option(TRACY_ENABLE "" ON)
add_subdirectory(external/tracy)

# stb is a header-only library; we create an 'interface' target for it
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE external/stb)

# tinyfiledialogs is a single-file C library
add_library(tinyfiledialogs STATIC
    external/tinyfiledialogs/tinyfiledialogs.c
)
target_include_directories(tinyfiledialogs PUBLIC external/tinyfiledialogs)

# Dear ImGui setup
set(IMGUI_DIR external/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui
    PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

# ImGui implementation requires Vulkan and GLFW
target_link_libraries(imgui
    PUBLIC
        glfw
        Vulkan::Vulkan
)

# ImGuizmo depends on the base ImGui target
add_library(imguizmo STATIC external/imguizmo/ImGuizmo.cpp)
target_include_directories(imguizmo PUBLIC external/imguizmo)
target_link_libraries(imguizmo PUBLIC imgui)

# Final linking
# PUBLIC: Dependencies that are needed in the engine's public headers 
# PRIVATE: Dependencies used only inside the engine's .cpp files
target_link_libraries(PXT_Engine
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm
        imgui
        imguizmo
        spdlog::spdlog_header_only

    PRIVATE 
        tinyobjloader
        stb
        Tracy::TracyClient
        Vulkan::shaderc_combined
        yaml-cpp
        tinyfiledialogs
)

# Compiler Flags & Definitions
target_compile_options(PXT_Engine
    PRIVATE
        # Enable warnings
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wshadow>
)

target_compile_definitions(PXT_Engine
    PUBLIC 
        # Global logging level
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE

        # Enable or disable validation layers based on build type
        $<$<CONFIG:Debug>:ENABLE_VALIDATION_LAYERS=1>
        $<$<NOT:$<CONFIG:Debug>>:ENABLE_VALIDATION_LAYERS=0>

)

# Speeds up compilation by pre-compiling the main engine header
target_precompile_headers(PXT_Engine PRIVATE src/core/pch.hpp)