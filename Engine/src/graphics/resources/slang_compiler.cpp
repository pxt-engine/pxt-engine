#include "graphics/resources/slang_compiler.hpp"
#include "graphics/resources/reflection_extractor.hpp"

namespace pxt {

    SlangCompiler& SlangCompiler::getInstance() {
        static SlangCompiler instance;
        return instance;
    }

    SlangCompiler::SlangCompiler() {
        // Create Slang global session
        SlangResult result = slang_createGlobalSession(SLANG_API_VERSION, m_globalSession.writeRef());

        if (SLANG_FAILED(result)) {
            PXT_FATAL("Failed to create Slang global session");
            return;
        }

        PXT_INFO("Slang global session created successfully");

        // Add default include path for assets/shaders
        const std::string cwd = std::filesystem::current_path().string();
        addIncludePath(cwd + "/assets/shaders");

        // Initialize the compilation session
        initializeSession();
    }

    SlangCompiler::~SlangCompiler() {
        // COM pointers will automatically release when they go out of scope
        if (m_session) {
            PXT_INFO("Releasing Slang session");
        }
        if (m_globalSession) {
            PXT_INFO("Releasing Slang global session");
        }
    }

    void SlangCompiler::initializeSession() {
        if (!m_globalSession) {
            PXT_ERROR("Cannot initialize session: global session is null");
            return;
        }

        // Configure session descriptor for Vulkan 1.3 and SPIR-V 1.4
        slang::SessionDesc sessionDesc = {};

        // Set up target descriptor for Vulkan/SPIR-V
        slang::TargetDesc targetDesc = {};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = m_globalSession->findProfile("spirv_1_4");
        targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;

        // Configure search paths for includes
        std::vector<const char*> searchPaths;
        searchPaths.reserve(m_includePaths.size());
        for (const auto& path : m_includePaths) {
            searchPaths.push_back(path.c_str());
        }

        sessionDesc.searchPaths = searchPaths.data();
        sessionDesc.searchPathCount = static_cast<SlangInt>(searchPaths.size());

        // Note: Preprocessor definitions are NOT set here in the session
        // They are applied per-compilation via specialization or other mechanisms
        // The session is kept clean to maximize caching and reuse

        // Create the session
        SlangResult result = m_globalSession->createSession(sessionDesc, m_session.writeRef());

        if (SLANG_FAILED(result)) {
            PXT_ERROR("Failed to create Slang compilation session");
            return;
        }

        PXT_INFO("Slang compilation session initialized with {} include path(s)", m_includePaths.size());
    }

    Slang::ComPtr<slang::ISession>
    SlangCompiler::createSessionWithDefinitions(const std::vector<std::pair<std::string, std::string>>& definitions) {

        if (!m_globalSession) {
            PXT_ERROR("Cannot create session: global session is null");
            return nullptr;
        }

        // Configure session descriptor for Vulkan 1.3 and SPIR-V 1.4
        slang::SessionDesc sessionDesc = {};

        // Set up target descriptor for Vulkan/SPIR-V
        slang::TargetDesc targetDesc = {};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = m_globalSession->findProfile("spirv_1_4");
        targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;

        // Configure search paths for includes
        std::vector<const char*> searchPaths;
        searchPaths.reserve(m_includePaths.size());
        for (const auto& path : m_includePaths) {
            searchPaths.push_back(path.c_str());
        }

        sessionDesc.searchPaths = searchPaths.data();
        sessionDesc.searchPathCount = static_cast<SlangInt>(searchPaths.size());

        // Configure preprocessor definitions
        std::vector<slang::PreprocessorMacroDesc> macros;
        macros.reserve(definitions.size());
        for (const auto& [name, value] : definitions) {
            slang::PreprocessorMacroDesc macro;
            macro.name = name.c_str();
            macro.value = value.c_str();
            macros.push_back(macro);
        }

        sessionDesc.preprocessorMacros = macros.data();
        sessionDesc.preprocessorMacroCount = static_cast<SlangInt>(macros.size());

        // Create the session with definitions
        Slang::ComPtr<slang::ISession> session;
        SlangResult result = m_globalSession->createSession(sessionDesc, session.writeRef());

        if (SLANG_FAILED(result)) {
            PXT_ERROR("Failed to create Slang session with preprocessor definitions");
            return nullptr;
        }

        return session;
    }

    void SlangCompiler::addIncludePath(const std::string& path) {
        // Check if path already exists
        auto it = std::find(m_includePaths.begin(), m_includePaths.end(), path);
        if (it != m_includePaths.end()) {
            PXT_WARN("Include path already exists: {}", path);
            return;
        }

        m_includePaths.push_back(path);
        PXT_INFO("Added include path: {}", path);

        // Reinitialize session with new include paths
        initializeSession();
    }

    void SlangCompiler::clearIncludePaths() {
        m_includePaths.clear();
        PXT_INFO("Cleared all include paths");

        // Reinitialize session without include paths
        initializeSession();
    }

    SlangCompiler::CompileResult
    SlangCompiler::compileFromFile(const std::string& filePath,
                                   const std::vector<std::pair<std::string, std::string>>& definitions) {

        CompileResult result;
        result.success = false;

        // Read shader source from file
        std::string source;
        try {
            source = readTextFile(filePath);
        } catch (const std::exception& e) {
            result.diagnostics = std::string("Failed to read file: ") + e.what();
            PXT_ERROR("{}", result.diagnostics);
            return result;
        }

        // Infer shader stage from file extension
        VkShaderStageFlagBits vkStage;
        try {
            vkStage = inferStageFromExtension(filePath);
        } catch (const std::exception& e) {
            result.diagnostics = std::string("Failed to infer shader stage: ") + e.what();
            PXT_ERROR("{}", result.diagnostics);
            return result;
        }

        SlangStage slangStage = mapVkStageToSlangStage(vkStage);

        // Get the appropriate session (with or without preprocessor definitions)
        Slang::ComPtr<slang::ISession> sessionToUse;
        if (!definitions.empty()) {
            // Create a temporary session with preprocessor definitions
            sessionToUse = createSessionWithDefinitions(definitions);
            if (!sessionToUse) {
                result.diagnostics = "Failed to create session with preprocessor definitions";
                PXT_ERROR("{}", result.diagnostics);
                return result;
            }
            PXT_INFO("Compiling {} with {} preprocessor definition(s)", filePath, definitions.size());
        } else {
            // Use the default session
            sessionToUse = m_session;
        }

        // Load the source code as a module
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        slang::IModule* module = sessionToUse->loadModuleFromSourceString(filePath.c_str(), filePath.c_str(),
                                                                          source.c_str(), diagnosticsBlob.writeRef());

        // Capture diagnostics
        if (diagnosticsBlob) {
            result.diagnostics = std::string(static_cast<const char*>(diagnosticsBlob->getBufferPointer()),
                                             diagnosticsBlob->getBufferSize());
        }

        if (!module) {
            result.success = false;
            PXT_ERROR("Slang compilation failed for {}: {}", filePath, result.diagnostics);
            return result;
        }

        // Create entry point (assume "main" as entry point)
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        SlangResult slangResult = module->findEntryPointByName("main", entryPoint.writeRef());

        if (SLANG_FAILED(slangResult)) {
            result.diagnostics += "\nFailed to find entry point 'main' in shader";
            PXT_ERROR("Failed to find entry point 'main' in {}", filePath);
            return result;
        }

        // Create a composite component type for linking
        std::vector<slang::IComponentType*> componentTypes = {module, entryPoint};
        Slang::ComPtr<slang::IComponentType> program;
        slangResult = sessionToUse->createCompositeComponentType(componentTypes.data(), componentTypes.size(),
                                                                 program.writeRef(), diagnosticsBlob.writeRef());

        if (diagnosticsBlob) {
            std::string linkDiagnostics(static_cast<const char*>(diagnosticsBlob->getBufferPointer()),
                                        diagnosticsBlob->getBufferSize());
            result.diagnostics += linkDiagnostics;
        }

        if (SLANG_FAILED(slangResult)) {
            result.success = false;
            PXT_ERROR("Slang program linking failed for {}: {}", filePath, result.diagnostics);
            return result;
        }

        // Compile to SPIR-V
        Slang::ComPtr<slang::IBlob> spirvCode;
        slangResult = program->getEntryPointCode(0, // Entry point index
                                                 0, // Target index (we only have one target)
                                                 spirvCode.writeRef(), diagnosticsBlob.writeRef());

        if (diagnosticsBlob) {
            std::string compileDiagnostics(static_cast<const char*>(diagnosticsBlob->getBufferPointer()),
                                           diagnosticsBlob->getBufferSize());
            result.diagnostics += compileDiagnostics;
        }

        if (SLANG_FAILED(slangResult) || !spirvCode) {
            result.success = false;
            PXT_ERROR("SPIR-V generation failed for {}: {}", filePath, result.diagnostics);
            return result;
        }

        // Copy SPIR-V bytecode to result
        const uint32_t* spirvData = static_cast<const uint32_t*>(spirvCode->getBufferPointer());
        size_t spirvSize = spirvCode->getBufferSize() / sizeof(uint32_t);
        result.spirv.assign(spirvData, spirvData + spirvSize);
        result.success = true;

        // Extract reflection metadata
        try {
            result.reflection = ReflectionExtractor::extract(program, 0, 0);
            if (!result.reflection) {
                PXT_WARN("Failed to extract reflection metadata for {}", filePath);
            }
        } catch (const std::exception& e) {
            PXT_ERROR("Exception during reflection extraction for {}: {}", filePath, e.what());
            result.reflection = nullptr;
        }

        PXT_INFO("Successfully compiled Slang shader: {} ({} bytes SPIR-V)", filePath, spirvCode->getBufferSize());

        return result;
    }

    SlangCompiler::CompileResult
    SlangCompiler::compileFromSource(const std::string& source, const std::string& fileName,
                                     VkShaderStageFlagBits stage,
                                     const std::vector<std::pair<std::string, std::string>>& definitions) {

        CompileResult result;
        result.success = false;

        // Convert Vulkan stage to Slang stage
        SlangStage slangStage = mapVkStageToSlangStage(stage);

        // Get the appropriate session (with or without preprocessor definitions)
        Slang::ComPtr<slang::ISession> sessionToUse;
        if (!definitions.empty()) {
            // Create a temporary session with preprocessor definitions
            sessionToUse = createSessionWithDefinitions(definitions);
            if (!sessionToUse) {
                result.diagnostics = "Failed to create session with preprocessor definitions";
                PXT_ERROR("{}", result.diagnostics);
                return result;
            }
            PXT_INFO("Compiling {} with {} preprocessor definition(s)", fileName, definitions.size());
        } else {
            // Use the default session
            sessionToUse = m_session;
        }

        // Load the source code as a module
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        slang::IModule* module = sessionToUse->loadModuleFromSourceString(fileName.c_str(), fileName.c_str(),
                                                                          source.c_str(), diagnosticsBlob.writeRef());

        // Capture diagnostics
        if (diagnosticsBlob) {
            result.diagnostics = std::string(static_cast<const char*>(diagnosticsBlob->getBufferPointer()),
                                             diagnosticsBlob->getBufferSize());
        }

        if (!module) {
            result.success = false;
            PXT_ERROR("Slang compilation failed for {}: {}", fileName, result.diagnostics);
            return result;
        }

        // Create entry point (assume "main" as entry point)
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        SlangResult slangResult = module->findEntryPointByName("main", entryPoint.writeRef());

        if (SLANG_FAILED(slangResult)) {
            result.diagnostics += "\nFailed to find entry point 'main' in shader";
            PXT_ERROR("Failed to find entry point 'main' in {}", fileName);
            return result;
        }

        // Create a composite component type for linking
        std::vector<slang::IComponentType*> componentTypes = {module, entryPoint};
        Slang::ComPtr<slang::IComponentType> program;
        slangResult = sessionToUse->createCompositeComponentType(componentTypes.data(), componentTypes.size(),
                                                                 program.writeRef(), diagnosticsBlob.writeRef());

        if (diagnosticsBlob) {
            std::string linkDiagnostics(static_cast<const char*>(diagnosticsBlob->getBufferPointer()),
                                        diagnosticsBlob->getBufferSize());
            result.diagnostics += linkDiagnostics;
        }

        if (SLANG_FAILED(slangResult)) {
            result.success = false;
            PXT_ERROR("Slang program linking failed for {}: {}", fileName, result.diagnostics);
            return result;
        }

        // Compile to SPIR-V
        Slang::ComPtr<slang::IBlob> spirvCode;
        slangResult = program->getEntryPointCode(0, // Entry point index
                                                 0, // Target index (we only have one target)
                                                 spirvCode.writeRef(), diagnosticsBlob.writeRef());

        if (diagnosticsBlob) {
            std::string compileDiagnostics(static_cast<const char*>(diagnosticsBlob->getBufferPointer()),
                                           diagnosticsBlob->getBufferSize());
            result.diagnostics += compileDiagnostics;
        }

        if (SLANG_FAILED(slangResult) || !spirvCode) {
            result.success = false;
            PXT_ERROR("SPIR-V generation failed for {}: {}", fileName, result.diagnostics);
            return result;
        }

        // Copy SPIR-V bytecode to result
        const uint32_t* spirvData = static_cast<const uint32_t*>(spirvCode->getBufferPointer());
        size_t spirvSize = spirvCode->getBufferSize() / sizeof(uint32_t);
        result.spirv.assign(spirvData, spirvData + spirvSize);
        result.success = true;

        // Extract reflection metadata
        try {
            result.reflection = ReflectionExtractor::extract(program, 0, 0);
            if (!result.reflection) {
                PXT_WARN("Failed to extract reflection metadata for {}", fileName);
            }
        } catch (const std::exception& e) {
            PXT_ERROR("Exception during reflection extraction for {}: {}", fileName, e.what());
            result.reflection = nullptr;
        }

        PXT_INFO("Successfully compiled Slang shader from source: {} ({} bytes SPIR-V)", fileName,
                 spirvCode->getBufferSize());

        return result;
    }

    VkShaderStageFlagBits SlangCompiler::inferStageFromExtension(const std::string& fileName) {
        // Find the last dot to extract extension
        size_t lastDot = fileName.find_last_of('.');
        if (lastDot == std::string::npos) {
            throw std::runtime_error("No file extension found in: " + fileName);
        }

        std::string extension = fileName.substr(lastDot + 1);

        // Map extension to Vulkan shader stage
        if (extension == "vert") {
            return VK_SHADER_STAGE_VERTEX_BIT;
        } else if (extension == "frag") {
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        } else if (extension == "comp") {
            return VK_SHADER_STAGE_COMPUTE_BIT;
        } else if (extension == "rgen") {
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        } else if (extension == "rchit") {
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        } else if (extension == "rmiss") {
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        } else if (extension == "rahit") {
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        } else if (extension == "rcall") {
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        } else if (extension == "rint") {
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        } else if (extension == "geom") {
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        } else if (extension == "tesc") {
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        } else if (extension == "tese") {
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        } else {
            throw std::runtime_error("Unrecognized shader extension: " + extension + " in file: " + fileName);
        }
    }

    SlangStage SlangCompiler::mapVkStageToSlangStage(VkShaderStageFlagBits vkStage) {
        switch (vkStage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return SLANG_STAGE_VERTEX;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return SLANG_STAGE_FRAGMENT;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return SLANG_STAGE_COMPUTE;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            return SLANG_STAGE_RAY_GENERATION;
        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            return SLANG_STAGE_CLOSEST_HIT;
        case VK_SHADER_STAGE_MISS_BIT_KHR:
            return SLANG_STAGE_MISS;
        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
            return SLANG_STAGE_ANY_HIT;
        case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
            return SLANG_STAGE_CALLABLE;
        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
            return SLANG_STAGE_INTERSECTION;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return SLANG_STAGE_GEOMETRY;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return SLANG_STAGE_HULL;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return SLANG_STAGE_DOMAIN;
        default:
            PXT_ERROR("Unsupported Vulkan shader stage: {}", static_cast<int>(vkStage));
            return SLANG_STAGE_NONE;
        }
    }

    std::string SlangCompiler::readTextFile(const std::string& filePath) {
        std::ifstream fileStream(filePath, std::ios::in);
        if (!fileStream.is_open()) {
            throw std::runtime_error("Could not open file: " + filePath);
        }

        std::stringstream buffer;
        buffer << fileStream.rdbuf();
        fileStream.close();

        return buffer.str();
    }

} // namespace pxt
