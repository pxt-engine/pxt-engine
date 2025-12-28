#pragma once

#include "core/pch.hpp"
#include "graphics/resources/texture2d.hpp"
#include "resources/types/image.hpp"

namespace pxt {

    class ResourceManager; // forward declaration

    class TextureImporter {
    public:
        static Unique<Image> import(const std::filesystem::path& filePath, ResourceInfo* resourceInfo = nullptr);
        static Unique<Texture2D> importTexture2D(const std::filesystem::path& filePath,
                                                 ResourceInfo* resourceInfo = nullptr);

        static void updateUi(ResourceInfo* resourceInfo);
    };
} // namespace pxt