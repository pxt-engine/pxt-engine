#pragma once

#include "core/pch.hpp"
#include "resources/types/mesh.hpp"

namespace pxt {

    class ResourceManager; // forward declaration

    class MeshImporter {
    public:
        static Shared<Mesh> importObj(ResourceManager& rm, const std::filesystem::path& filePath,
                                      ResourceInfo* resourceInfo = nullptr);

        static void updateUi(ResourceInfo* resourceInfo);
    };
} // namespace pxt
