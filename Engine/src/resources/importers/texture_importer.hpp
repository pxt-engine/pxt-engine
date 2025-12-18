#pragma once

#include "core/pch.hpp"
#include "resources/types/image.hpp"

namespace pxt {

    class ResourceManager; // forward declaration

    class TextureImporter {
    public:
        static Shared<Image> import(ResourceManager& rm, const std::filesystem::path& filePath,
                                    ResourceInfo* resourceInfo = nullptr);

        static void updateUi(ResourceInfo* resourceInfo);
    };
} // namespace pxt