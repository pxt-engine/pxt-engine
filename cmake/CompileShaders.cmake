# cmake/CompileShaders.cmake

function(compile_shaders TARGET_NAME SHADER_DIR OUTPUT_DIR)
    # Find the validator once if not already found
    find_program(GLSL_VALIDATOR glslangValidator HINTS 
        ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} 
        $ENV{VULKAN_SDK}/Bin
        $ENV{VULKAN_SDK}/Bin32
    )

    if(NOT GLSL_VALIDATOR)
        message(FATAL_ERROR "glslangValidator not found! Cannot compile shaders.")
    endif()

    # Define supported extensions
    set(EXTENSIONS "*.frag" "*.vert" "*.comp" "*.geom" "*.tesc" "*.tese" 
                   "*.rgen" "*.rahit" "*.rchit" "*.rmiss" "*.rint" "*.rcall")

    # Prefix each extension with the shader directory path
    set(GLOB_EXPRESSIONS)
    foreach(EXT ${EXTENSIONS})
        list(APPEND GLOB_EXPRESSIONS "${SHADER_DIR}/${EXT}")
        # Support recursive raytracing folder specifically if needed, 
        # or just use GLSL_SOURCE_FILES recursive glob
    endforeach()

    file(GLOB_RECURSE GLSL_SOURCE_FILES ${GLOB_EXPRESSIONS})
    file(MAKE_DIRECTORY ${OUTPUT_DIR})

    set(SPIRV_BINARY_FILES "")

    foreach(GLSL ${GLSL_SOURCE_FILES})
        get_filename_component(FILE_NAME ${GLSL} NAME)
        set(SPIRV "${OUTPUT_DIR}/${FILE_NAME}.spv")

        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV} 
                    -I${SHADER_DIR} --target-env vulkan1.3
            DEPENDS ${GLSL}
            COMMENT "Compiling shader: ${FILE_NAME}"
        )
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach()

    # Create a unique target for these shaders
    add_custom_target(${TARGET_NAME}_Shaders DEPENDS ${SPIRV_BINARY_FILES})
    
    # Link to the main target
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_Shaders)
endfunction()