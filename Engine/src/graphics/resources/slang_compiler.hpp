#pragma once

#include "core/pch.hpp"
#include "graphics/resources/shader_reflection.hpp"

#include <slang-com-ptr.h>
#include <slang.h>

namespace pxt {

    /**
     * @brief Singleton class that manages Slang shader compilation sessions.
     *
     * SlangCompiler provides a simplified facade over Slang's COM-style API for compiling
     * Slang shaders to SPIR-V with Vulkan 1.3 and SPIR-V 1.4 targets. It manages a single
     * global session for caching and performance, and handles include path resolution for
     * the assets/shaders directory structure.
     *
     * The class follows the singleton pattern to ensure a single Slang global session
     * is shared across all shader compilations, enabling module caching and reducing
     * compilation overhead.
     *
     * @note This class is thread-safe for getInstance() but compilation methods are not
     *       thread-safe and should be called from a single thread or externally synchronized.
     */
    class SlangCompiler {
    public:
        /**
         * @brief Result of shader compilation containing SPIR-V bytecode and diagnostics.
         */
        struct CompileResult {
            std::vector<uint32_t> spirv;         ///< Compiled SPIR-V bytecode
            Shared<ShaderReflection> reflection; ///< Extracted shader reflection metadata
            std::string diagnostics;             ///< Compilation diagnostics (errors and warnings)
            bool success;                        ///< True if compilation succeeded, false otherwise
        };

        /**
         * @brief Get the singleton instance of SlangCompiler.
         * @return Reference to the singleton instance.
         */
        static SlangCompiler& getInstance();

        // Delete copy and move constructors/operators (singleton pattern)
        SlangCompiler(const SlangCompiler&) = delete;
        SlangCompiler& operator=(const SlangCompiler&) = delete;
        SlangCompiler(SlangCompiler&&) = delete;
        SlangCompiler& operator=(SlangCompiler&&) = delete;

        /**
         * @brief Add an include search path for shader compilation.
         *
         * Include paths are used to resolve #include directives in shader source code.
         * Paths should be absolute or relative to the current working directory.
         *
         * @param path The directory path to add to the include search paths.
         */
        void addIncludePath(const std::string& path);

        /**
         * @brief Clear all include search paths.
         *
         * Removes all previously added include paths. The session will need to be
         * reinitialized with new paths before compilation.
         */
        void clearIncludePaths();

        /**
         * @brief Get the current list of include search paths.
         * @return Const reference to the vector of include paths.
         */
        const std::vector<std::string>& getIncludePaths() const { return m_includePaths; }

        /**
         * @brief Compile a Slang shader from a file path.
         *
         * Loads the shader source from the specified file, infers the shader stage from
         * the file extension (.slang.vert, .slang.frag, etc.), and compiles to SPIR-V.
         *
         * @param filePath Path to the Slang shader file (relative or absolute)
         * @param definitions Optional preprocessor definitions as name-value pairs
         * @return CompileResult containing SPIR-V bytecode, diagnostics, and success flag
         */
        CompileResult compileFromFile(const std::string& filePath,
                                      const std::vector<std::pair<std::string, std::string>>& definitions = {});

        /**
         * @brief Compile a Slang shader from an in-memory source string.
         *
         * Compiles the provided shader source code to SPIR-V. The fileName parameter
         * is used only for diagnostic messages and does not need to correspond to an
         * actual file on disk.
         *
         * @param source The Slang shader source code as a string
         * @param fileName Name to use in diagnostic messages (e.g., "generated.slang")
         * @param stage The shader stage (vertex, fragment, compute, etc.)
         * @param definitions Optional preprocessor definitions as name-value pairs
         * @return CompileResult containing SPIR-V bytecode, diagnostics, and success flag
         */
        CompileResult compileFromSource(const std::string& source, const std::string& fileName,
                                        VkShaderStageFlagBits stage,
                                        const std::vector<std::pair<std::string, std::string>>& definitions = {});

    private:
        /**
         * @brief Private constructor for singleton pattern.
         *
         * Initializes the Slang global session and creates a compilation session
         * with Vulkan 1.3 and SPIR-V 1.4 target configuration.
         */
        SlangCompiler();

        /**
         * @brief Destructor - releases Slang session resources.
         */
        ~SlangCompiler();

        /**
         * @brief Initialize or reinitialize the Slang session with current configuration.
         *
         * Creates a new session with Vulkan 1.3 target, SPIR-V 1.4 output format,
         * and the current include paths. Called during construction and when
         * include paths are modified.
         */
        void initializeSession();

        /**
         * @brief Create a temporary session with preprocessor definitions.
         *
         * Creates a new Slang session with the same configuration as the default session,
         * but with additional preprocessor definitions applied. This session is used
         * for compilations that require specific preprocessor macros.
         *
         * @param definitions Vector of name-value pairs for preprocessor definitions
         * @return ComPtr to the created session, or nullptr on failure
         */
        Slang::ComPtr<slang::ISession>
        createSessionWithDefinitions(const std::vector<std::pair<std::string, std::string>>& definitions);

        /**
         * @brief Infer shader stage from file extension.
         *
         * Supports extensions: .slang.vert, .slang.frag, .slang.comp, .slang.rgen,
         * .slang.rchit, .slang.rmiss, .slang.rahit, .slang.rcall, .slang.rint
         *
         * @param fileName The shader file name or path
         * @return VkShaderStageFlagBits corresponding to the file extension
         * @throws std::runtime_error if extension is not recognized
         */
        VkShaderStageFlagBits inferStageFromExtension(const std::string& fileName);

        /**
         * @brief Map Vulkan shader stage to Slang stage enum.
         *
         * @param vkStage Vulkan shader stage flag bits
         * @return Corresponding Slang stage enum value
         */
        SlangStage mapVkStageToSlangStage(VkShaderStageFlagBits vkStage);

        /**
         * @brief Read text file contents into a string.
         *
         * @param filePath Path to the file to read
         * @return File contents as a string
         * @throws std::runtime_error if file cannot be opened or read
         */
        std::string readTextFile(const std::string& filePath);

        // Slang COM objects
        Slang::ComPtr<slang::IGlobalSession> m_globalSession;
        Slang::ComPtr<slang::ISession> m_session;

        // Include path management
        std::vector<std::string> m_includePaths;
    };

} // namespace pxt
