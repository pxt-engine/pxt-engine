#include "graphics/resources/slang_compiler.hpp"
#include <iostream>

int main() {
    using namespace pxt;

    // Get the SlangCompiler instance
    SlangCompiler& compiler = SlangCompiler::getInstance();

    std::cout << "Testing Slang preprocessor definitions..." << std::endl;

    // Test 1: Compile without definitions
    std::cout << "\n=== Test 1: Compile without definitions ===" << std::endl;
    auto result1 = compiler.compileFromFile("assets/shaders/test_preprocessor.slang.vert");
    if (result1.success) {
        std::cout << "SUCCESS: Compiled without definitions (" << result1.spirv.size() * 4 << " bytes)" << std::endl;
    } else {
        std::cout << "FAILED: " << result1.diagnostics << std::endl;
        return 1;
    }

    // Test 2: Compile with USE_COLOR definition
    std::cout << "\n=== Test 2: Compile with USE_COLOR definition ===" << std::endl;
    std::vector<std::pair<std::string, std::string>> definitions2 = {
        {"USE_COLOR", "1"}
    };
    auto result2 = compiler.compileFromFile("assets/shaders/test_preprocessor.slang.vert", definitions2);
    if (result2.success) {
        std::cout << "SUCCESS: Compiled with USE_COLOR (" << result2.spirv.size() * 4 << " bytes)" << std::endl;
    } else {
        std::cout << "FAILED: " << result2.diagnostics << std::endl;
        return 1;
    }

    // Test 3: Compile with multiple definitions
    std::cout << "\n=== Test 3: Compile with multiple definitions ===" << std::endl;
    std::vector<std::pair<std::string, std::string>> definitions3 = {
        {"USE_COLOR", "1"},
        {"SCALE_FACTOR", "2.0"}
    };
    auto result3 = compiler.compileFromFile("assets/shaders/test_preprocessor.slang.vert", definitions3);
    if (result3.success) {
        std::cout << "SUCCESS: Compiled with USE_COLOR and SCALE_FACTOR (" << result3.spirv.size() * 4 << " bytes)" << std::endl;
    } else {
        std::cout << "FAILED: " << result3.diagnostics << std::endl;
        return 1;
    }

    // Test 4: Compile from source with definitions
    std::cout << "\n=== Test 4: Compile from source with definitions ===" << std::endl;
    const char* testSource = R"(
#version 450

#ifdef TEST_DEFINE
    #define VALUE 42.0
#else
    #define VALUE 0.0
#endif

layout(location = 0) in vec3 inPosition;
layout(location = 0) out float outValue;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    outValue = VALUE;
}
)";
    std::vector<std::pair<std::string, std::string>> definitions4 = {
        {"TEST_DEFINE", ""}
    };
    auto result4 = compiler.compileFromSource(testSource, "test_source.slang.vert", VK_SHADER_STAGE_VERTEX_BIT, definitions4);
    if (result4.success) {
        std::cout << "SUCCESS: Compiled from source with TEST_DEFINE (" << result4.spirv.size() * 4 << " bytes)" << std::endl;
    } else {
        std::cout << "FAILED: " << result4.diagnostics << std::endl;
        return 1;
    }

    std::cout << "\n=== All tests passed! ===" << std::endl;
    return 0;
}
