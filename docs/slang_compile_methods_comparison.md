# SlangCompiler Methods Comparison

## Overview

The `SlangCompiler` class provides two methods for compiling Slang shaders to SPIR-V:
1. `compileFromFile()` - Compiles from a file on disk
2. `compileFromSource()` - Compiles from an in-memory source string

This document compares these methods and provides guidance on when to use each.

## Method Signatures

### compileFromFile()
```cpp
CompileResult compileFromFile(
    const std::string& filePath,
    const std::vector<std::pair<std::string, std::string>>& definitions = {}
);
```

### compileFromSource()
```cpp
CompileResult compileFromSource(
    const std::string& source,
    const std::string& fileName,
    VkShaderStageFlagBits stage,
    const std::vector<std::pair<std::string, std::string>>& definitions = {}
);
```

## Detailed Comparison

| Feature | compileFromFile() | compileFromSource() |
|---------|-------------------|---------------------|
| **Input Source** | File path (string) | Source code (string) |
| **File I/O** | Reads file from disk | No file access |
| **Stage Detection** | Automatic (from extension) | Manual (parameter) |
| **fileName Parameter** | Actual file path | Diagnostic label only |
| **Use Case** | Asset pipeline, file-based shaders | Runtime generation, testing |
| **Include Resolution** | Relative to file location | Relative to working directory |
| **Error Messages** | Reference actual file path | Reference provided fileName |
| **Performance** | File I/O overhead | No I/O overhead |
| **Caching** | Can cache by file path | Must cache by content hash |

## Implementation Differences

### compileFromFile() Flow
```
1. Read file from disk (readTextFile)
2. Infer shader stage from extension (inferStageFromExtension)
3. Load module from source string
4. Find entry point
5. Link program
6. Generate SPIR-V
7. Return result
```

### compileFromSource() Flow
```
1. Use provided source string (no file I/O)
2. Use provided shader stage (no inference)
3. Load module from source string
4. Find entry point
5. Link program
6. Generate SPIR-V
7. Return result
```

**Note**: Steps 3-7 are identical in both methods.

## When to Use Each Method

### Use compileFromFile() when:
- ✅ Loading shaders from the asset pipeline
- ✅ Working with file-based shader development workflow
- ✅ Shader stage can be inferred from file extension
- ✅ Shader source is stored on disk
- ✅ You want automatic stage detection
- ✅ Building offline shader compilation tools

### Use compileFromSource() when:
- ✅ Generating shaders at runtime
- ✅ Preprocessing shader source before compilation
- ✅ Testing shaders without creating files
- ✅ Composing shaders from multiple sources
- ✅ Implementing shader hot-reload with modifications
- ✅ Creating shader variants programmatically
- ✅ Working with embedded shader source code

## Code Examples

### Example 1: Loading from Asset Pipeline

**Using compileFromFile():**
```cpp
auto& compiler = SlangCompiler::getInstance();
auto result = compiler.compileFromFile("assets/shaders/material.slang.frag");

if (result.success) {
    // Create shader module from SPIR-V
    createShaderModule(result.spirv);
}
```

**Using compileFromSource():**
```cpp
auto& compiler = SlangCompiler::getInstance();

// Read file manually
std::ifstream file("assets/shaders/material.slang.frag");
std::string source((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());

auto result = compiler.compileFromSource(
    source,
    "material.slang.frag",
    VK_SHADER_STAGE_FRAGMENT_BIT
);

if (result.success) {
    createShaderModule(result.spirv);
}
```

**Recommendation**: Use `compileFromFile()` - simpler and more efficient.

### Example 2: Runtime Shader Generation

**Using compileFromFile():**
```cpp
// Generate shader source
std::string shaderSource = generateLightingShader(numLights);

// Write to temporary file
std::ofstream tempFile("temp_shader.slang.frag");
tempFile << shaderSource;
tempFile.close();

// Compile from file
auto& compiler = SlangCompiler::getInstance();
auto result = compiler.compileFromFile("temp_shader.slang.frag");

// Clean up
std::remove("temp_shader.slang.frag");
```

**Using compileFromSource():**
```cpp
// Generate shader source
std::string shaderSource = generateLightingShader(numLights);

// Compile directly
auto& compiler = SlangCompiler::getInstance();
auto result = compiler.compileFromSource(
    shaderSource,
    "generated_lighting.slang",
    VK_SHADER_STAGE_FRAGMENT_BIT
);
```

**Recommendation**: Use `compileFromSource()` - no file I/O, cleaner code.

### Example 3: Shader Preprocessing

**Using compileFromFile():**
```cpp
// Read original shader
std::string source = readFile("shader.slang.frag");

// Preprocess
std::string preprocessed = expandMacros(source);

// Write preprocessed version
writeFile("shader_preprocessed.slang.frag", preprocessed);

// Compile
auto result = compiler.compileFromFile("shader_preprocessed.slang.frag");
```

**Using compileFromSource():**
```cpp
// Read original shader
std::string source = readFile("shader.slang.frag");

// Preprocess
std::string preprocessed = expandMacros(source);

// Compile directly
auto result = compiler.compileFromSource(
    preprocessed,
    "shader.slang.frag",
    VK_SHADER_STAGE_FRAGMENT_BIT
);
```

**Recommendation**: Use `compileFromSource()` - avoids temporary files.

### Example 4: Unit Testing

**Using compileFromFile():**
```cpp
TEST(ShaderCompilation, BasicFragmentShader) {
    // Create test file
    std::ofstream testFile("test_shader.slang.frag");
    testFile << R"(
        [shader("fragment")]
        float4 main() : SV_Target {
            return float4(1, 0, 0, 1);
        }
    )";
    testFile.close();

    auto& compiler = SlangCompiler::getInstance();
    auto result = compiler.compileFromFile("test_shader.slang.frag");

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.spirv.size(), 0);

    std::remove("test_shader.slang.frag");
}
```

**Using compileFromSource():**
```cpp
TEST(ShaderCompilation, BasicFragmentShader) {
    const char* testShader = R"(
        [shader("fragment")]
        float4 main() : SV_Target {
            return float4(1, 0, 0, 1);
        }
    )";

    auto& compiler = SlangCompiler::getInstance();
    auto result = compiler.compileFromSource(
        testShader,
        "test_shader.slang",
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.spirv.size(), 0);
}
```

**Recommendation**: Use `compileFromSource()` - cleaner tests, no file cleanup.

## Performance Considerations

### compileFromFile()
- **File I/O overhead**: Reading from disk adds latency
- **File system access**: May be slow on network drives or with antivirus
- **Caching**: Can cache by file path and modification time
- **Best for**: Infrequent compilation of stable shaders

### compileFromSource()
- **No I/O overhead**: Source is already in memory
- **Direct compilation**: Minimal overhead before Slang processing
- **Caching**: Requires content hashing for cache keys
- **Best for**: Frequent recompilation or runtime generation

## Error Handling

Both methods return the same `CompileResult` structure:

```cpp
struct CompileResult {
    std::vector<uint32_t> spirv;  // Compiled SPIR-V bytecode
    std::string diagnostics;       // Compilation diagnostics
    bool success;                  // Compilation success flag
};
```

### Error Message Differences

**compileFromFile():**
```
PXT_ERROR("Slang compilation failed for assets/shaders/material.slang.frag: 
           error: undeclared identifier 'foo'")
```

**compileFromSource():**
```
PXT_ERROR("Slang compilation failed for generated_shader.slang: 
           error: undeclared identifier 'foo'")
```

The fileName in `compileFromSource()` is purely for diagnostic purposes and doesn't need to correspond to an actual file.

## Best Practices

### For compileFromFile()
1. Use for asset pipeline shaders
2. Ensure file extensions match shader stages
3. Handle file not found errors
4. Consider caching compiled SPIR-V by file path

### For compileFromSource()
1. Use meaningful fileName for diagnostics
2. Always specify the correct shader stage
3. Consider caching by source content hash
4. Validate generated source before compilation

## Migration Guide

### Converting from compileFromFile() to compileFromSource()

```cpp
// Before
auto result = compiler.compileFromFile("shader.slang.frag");

// After
std::string source = readTextFile("shader.slang.frag");
auto result = compiler.compileFromSource(
    source,
    "shader.slang.frag",
    VK_SHADER_STAGE_FRAGMENT_BIT
);
```

### Converting from compileFromSource() to compileFromFile()

```cpp
// Before
auto result = compiler.compileFromSource(
    shaderSource,
    "shader.slang",
    VK_SHADER_STAGE_FRAGMENT_BIT
);

// After
std::ofstream file("shader.slang.frag");
file << shaderSource;
file.close();

auto result = compiler.compileFromFile("shader.slang.frag");
```

## Common Patterns

### Pattern 1: Shader Variants
```cpp
std::vector<std::string> variants = {
    "#define USE_NORMAL_MAPPING\n",
    "#define USE_PARALLAX_MAPPING\n",
    "#define USE_BOTH\n"
};

std::string baseShader = readTextFile("material.slang.frag");

for (const auto& variant : variants) {
    std::string source = variant + baseShader;
    auto result = compiler.compileFromSource(
        source,
        "material_variant.slang",
        VK_SHADER_STAGE_FRAGMENT_BIT
    );
    // Store compiled variant
}
```

### Pattern 2: Shader Hot-Reload with Modifications
```cpp
void reloadShaderWithModifications(const std::string& filePath) {
    // Read original
    std::string source = readTextFile(filePath);
    
    // Apply runtime modifications
    source = injectDebugCode(source);
    
    // Compile modified version
    VkShaderStageFlagBits stage = inferStageFromPath(filePath);
    auto result = compiler.compileFromSource(source, filePath, stage);
    
    if (result.success) {
        updateShaderModule(result.spirv);
    }
}
```

### Pattern 3: Shader Composition
```cpp
std::string composeShader(
    const std::string& header,
    const std::string& body,
    const std::string& footer
) {
    std::string composed = header + "\n" + body + "\n" + footer;
    
    return compiler.compileFromSource(
        composed,
        "composed.slang",
        VK_SHADER_STAGE_FRAGMENT_BIT
    );
}
```

## Summary

Both methods serve important but different purposes:

- **compileFromFile()**: Best for traditional file-based shader workflows
- **compileFromSource()**: Best for dynamic, runtime shader scenarios

Choose based on your specific use case, with the general guideline:
- If the shader exists as a file → use `compileFromFile()`
- If the shader is generated or modified at runtime → use `compileFromSource()`
