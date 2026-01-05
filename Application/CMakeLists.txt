project(${PROJECT_NAME} LANGUAGES CXX)

add_executable(${PROJECT_NAME})

# Specify the source and header files for the application
file(GLOB_RECURSE APP_SOURCES
    src/*.cpp
    src/*.hpp
)

target_sources(${PROJECT_NAME} PRIVATE ${APP_SOURCES})

# Link against the engine and editor modules
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        PXT_Engine
        PXT_Editor
)

# Set the working directory for debugging in Visual Studio
set_property(TARGET ${PROJECT_NAME} PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/out"
)