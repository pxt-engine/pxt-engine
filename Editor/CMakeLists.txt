project(PXT_Editor LANGUAGES CXX)

add_library(PXT_Editor STATIC)

file(GLOB_RECURSE EDITOR_SOURCES
    src/*.cpp
    src/*.hpp
)

target_sources(PXT_Editor PRIVATE ${EDITOR_SOURCES})

target_include_directories(PXT_Editor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(PXT_Editor
    PRIVATE
        PXT_Engine
)

target_compile_options(PXT_Editor
    PRIVATE
        # Enable warnings
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wshadow>
)

set_property(TARGET PXT_Editor PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/out"
)