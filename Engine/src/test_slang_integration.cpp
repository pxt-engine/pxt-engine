// Test file to verify Slang library integration
// This file will be removed after verification

#include <slang.h>
#include <slang-com-ptr.h>
#include <iostream>

#include "graphics/resources/slang_compiler.hpp"

namespace pxt {

// Simple test function to verify Slang headers are accessible and linkable
bool testSlangIntegration() {
    // Test 1: Verify we can create a global session
    slang::IGlobalSession* globalSession = nullptr;
    SlangResult result = slang_createGlobalSession(SLANG_API_VERSION, &globalSession);
    
    if (SLANG_FAILED(result)) {
        std::cerr << "Failed to create Slang global session" << std::endl;
        return false;
    }
    
    std::cout << "✓ Slang global session created successfully" << std::endl;
    
    // Test 2: Verify we can query session information
    if (globalSession) {
        // Clean up
        globalSession->release();
        std::cout << "✓ Slang session released successfully" << std::endl;
    }
    
    std::cout << "✓ Slang integration test passed!" << std::endl;
    return true;
}

// Test function to verify SlangCompiler singleton
bool testSlangCompiler() {
    std::cout << "\n=== Testing SlangCompiler Singleton ===" << std::endl;
    
    // Test 1: Get singleton instance
    SlangCompiler& compiler = SlangCompiler::getInstance();
    std::cout << "✓ SlangCompiler singleton instance obtained" << std::endl;
    
    // Test 2: Verify default include path is set
    const auto& paths = compiler.getIncludePaths();
    if (paths.empty()) {
        std::cerr << "✗ No default include paths set" << std::endl;
        return false;
    }
    std::cout << "✓ Default include paths configured (" << paths.size() << " path(s))" << std::endl;
    for (const auto& path : paths) {
        std::cout << "  - " << path << std::endl;
    }
    
    // Test 3: Add a custom include path
    compiler.addIncludePath("test/path");
    if (compiler.getIncludePaths().size() != paths.size() + 1) {
        std::cerr << "✗ Failed to add include path" << std::endl;
        return false;
    }
    std::cout << "✓ Custom include path added successfully" << std::endl;
    
    // Test 4: Clear include paths
    compiler.clearIncludePaths();
    if (!compiler.getIncludePaths().empty()) {
        std::cerr << "✗ Failed to clear include paths" << std::endl;
        return false;
    }
    std::cout << "✓ Include paths cleared successfully" << std::endl;
    
    // Test 5: Restore default path
    const std::string cwd = std::filesystem::current_path().string();
    compiler.addIncludePath(cwd + "/assets/shaders");
    std::cout << "✓ Default include path restored" << std::endl;
    
    std::cout << "✓ SlangCompiler test passed!" << std::endl;
    return true;
}

} // namespace pxt
