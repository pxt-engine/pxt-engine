project(${PROJECT_NAME} LANGUAGES CXX)

add_executable(${PROJECT_NAME})

file(GLOB_RECURSE APP_SOURCES src/*.cpp)
target_sources(${PROJECT_NAME} PRIVATE ${APP_SOURCES})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        PXT_Engine
        PXT_Editor
)

set_property(TARGET ${PROJECT_NAME} PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/out"
)