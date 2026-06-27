// Example: Using SlangCompiler::compileFromSource()
// This demonstrates how to compile Slang shaders from in-memory source strings

#include "graphics/resources/slang_compiler.hpp"
#include <iostream>

namespace pxt {

// Example 1: Simple fragment shader compilation
void example_simple_fragment_shader() {
    const char* fragmentSource = R"(
        [shader("fragment")]
        float4 main() : SV_Target {
            return float4(1.0, 0.0, 0.0, 1.0); // Red color
        }
    )";

    auto& compiler = SlangCompiler::getInstance();
    auto result = compiler.compileFromSource(
        fragmentSource,
        "simple_red.slang",
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    if (result.success) {
        std::cout << "✓ Compiled simple fragment shader: " 
                  << result.spirv.size() << " SPIR-V words\n";
    } else {
        std::cerr << "✗ Compilation failed:\n" << result.diagnostics << "\n";
    }
}

// Example 2: Vertex shader with input/output
void example_vertex_shader() {
    const char* vertexSource = R"(
        struct VertexInput {
            float3 position : POSITION;
            float3 normal : NORMAL;
        };

        struct VertexOutput {
            float4 position : SV_Position;
            float3 normal : NORMAL;
        };

        cbuffer TransformUBO : register(b0) {
            float4x4 modelViewProj;
        };

        [shader("vertex")]
        VertexOutput main(VertexInput input) {
            VertexOutput output;
            output.position = mul(modelViewProj, float4(input.position, 1.0));
            output.normal = input.normal;
            return output;
        }
    )";

    auto& compiler = SlangCompiler::getInstance();
    auto result = compiler.compileFromSource(
        vertexSource,
        "transform.slang",
        VK_SHADER_STAGE_VERTEX_BIT
    );

    if (result.success) {
        std::cout << "✓ Compiled vertex shader: " 
                  << result.spirv.size() << " SPIR-V words\n";
    } else {
        std::cerr << "✗ Compilation failed:\n" << result.diagnostics << "\n";
    }
}

// Example 3: Compute shader
void example_compute_shader() {
    const char* computeSource = R"(
        RWStructuredBuffer<float4> outputBuffer : register(u0);

        [shader("compute")]
        [numthreads(16, 16, 1)]
        void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
            uint index = dispatchThreadID.y * 256 + dispatchThreadID.x;
            outputBuffer[index] = float4(
                float(dispatchThreadID.x) / 256.0,
                float(dispatchThreadID.y) / 256.0,
                0.0,
                1.0
            );
        }
    )";

    auto& compiler = SlangCompiler::getInstance();
    auto result = compiler.compileFromSource(
        computeSource,
        "gradient.slang",
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    if (result.success) {
        std::cout << "✓ Compiled compute shader: " 
                  << result.spirv.size() << " SPIR-V words\n";
    } else {
        std::cerr << "✗ Compilation failed:\n" << result.diagnostics << "\n";
    }
}

// Example 4: Runtime shader generation
std::string generateProceduralShader(int numLights) {
    std::stringstream ss;
    ss << "[shader(\"fragment\")]\n";
    ss << "float4 main(float3 worldPos : POSITION) : SV_Target {\n";
    ss << "    float3 color = float3(0, 0, 0);\n";
    
    for (int i = 0; i < numLights; ++i) {
        ss << "    // Light " << i << "\n";
        ss << "    color += float3(0.1, 0.1, 0.1);\n";
    }
    
    ss << "    return float4(color, 1.0);\n";
    ss << "}\n";
    
    return ss.str();
}

void example_runtime_generation() {
    std::string generatedShader = generateProceduralShader(5);
    
    auto& compiler = SlangCompiler::getInstance();
    auto result = compiler.compileFromSource(
        generatedShader,
        "generated_lighting.slang",
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    if (result.success) {
        std::cout << "✓ Compiled generated shader: " 
                  << result.spirv.size() << " SPIR-V words\n";
    } else {
        std::cerr << "✗ Compilation failed:\n" << result.diagnostics << "\n";
    }
}

// Example 5: Error handling
void example_error_handling() {
    const char* invalidSource = R"(
        [shader("fragment")]
        float4 main() : SV_Target {
            return undeclared_variable; // This will cause an error
        }
    )";

    auto& compiler = SlangCompiler::getInstance();
    auto result = compiler.compileFromSource(
        invalidSource,
        "invalid.slang",
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    if (!result.success) {
        std::cout << "✓ Error correctly detected\n";
        std::cout << "Diagnostics:\n" << result.diagnostics << "\n";
    } else {
        std::cerr << "✗ Expected compilation to fail\n";
    }
}

// Example 6: Ray tracing shader
void example_ray_generation_shader() {
    const char* rayGenSource = R"(
        RWTexture2D<float4> outputImage : register(u0);
        RaytracingAccelerationStructure scene : register(t0);

        struct RayPayload {
            float3 color;
        };

        [shader("raygeneration")]
        void main() {
            uint2 launchIndex = DispatchRaysIndex().xy;
            uint2 launchDim = DispatchRaysDimensions().xy;

            float2 uv = (float2(launchIndex) + 0.5) / float2(launchDim);
            
            RayDesc ray;
            ray.Origin = float3(0, 0, -5);
            ray.Direction = normalize(float3(uv * 2 - 1, 1));
            ray.TMin = 0.001;
            ray.TMax = 1000.0;

            RayPayload payload;
            payload.color = float3(0, 0, 0);

            TraceRay(scene, 0, 0xFF, 0, 0, 0, ray, payload);

            outputImage[launchIndex] = float4(payload.color, 1.0);
        }
    )";

    auto& compiler = SlangCompiler::getInstance();
    auto result = compiler.compileFromSource(
        rayGenSource,
        "raygen.slang",
        VK_SHADER_STAGE_RAYGEN_BIT_KHR
    );

    if (result.success) {
        std::cout << "✓ Compiled ray generation shader: " 
                  << result.spirv.size() << " SPIR-V words\n";
    } else {
        std::cerr << "✗ Compilation failed:\n" << result.diagnostics << "\n";
    }
}

} // namespace pxt

// Main function to run all examples
int main() {
    std::cout << "=== SlangCompiler::compileFromSource() Examples ===\n\n";

    std::cout << "Example 1: Simple Fragment Shader\n";
    pxt::example_simple_fragment_shader();
    std::cout << "\n";

    std::cout << "Example 2: Vertex Shader with Uniforms\n";
    pxt::example_vertex_shader();
    std::cout << "\n";

    std::cout << "Example 3: Compute Shader\n";
    pxt::example_compute_shader();
    std::cout << "\n";

    std::cout << "Example 4: Runtime Shader Generation\n";
    pxt::example_runtime_generation();
    std::cout << "\n";

    std::cout << "Example 5: Error Handling\n";
    pxt::example_error_handling();
    std::cout << "\n";

    std::cout << "Example 6: Ray Generation Shader\n";
    pxt::example_ray_generation_shader();
    std::cout << "\n";

    return 0;
}
