project(PXT_Editor LANGUAGES CXX)

# Create a static library for the Editor
add_library(PXT_Editor STATIC)

# Automatically collect all source and header files
file(GLOB_RECURSE EDITOR_SOURCES
    src/*.cpp
    src/*.hpp
)

target_sources(PXT_Editor PRIVATE ${EDITOR_SOURCES})

# Include directories
target_include_directories(PXT_Editor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Linking
target_link_libraries(PXT_Editor
    PRIVATE
        PXT_Engine
)

