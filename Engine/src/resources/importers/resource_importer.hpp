#pragma once

#include "core/pch.hpp"
#include "resources/resource.hpp"

namespace PXTEngine {

	class ResourceManager; // forward declaration

    class ResourceImporter {
    public:
        Shared<Resource> import(ResourceManager& rm, const std::filesystem::path& filePath,
            ResourceInfo* resourceInfo = nullptr);
    };
}