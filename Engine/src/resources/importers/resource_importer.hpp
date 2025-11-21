#pragma once

#include "core/pch.hpp"
#include "core/filesystem.hpp"
#include "resources/resource.hpp"
#include "resources/importers/texture_importer.hpp"
#include "resources/importers/mesh_importer.hpp"

namespace PXTEngine {

	class ResourceManager; // forward declaration

    using ResourceImportFunction = std::function<Shared<Resource>(
        ResourceManager&,
        const std::filesystem::path&,
        ResourceInfo* resourceInfo
    )>;

	using ResourceImportUiFunction = std::function<void(ResourceInfo* resourceInfo)>;

	using ResourceInfoDefaultConstructor = std::function<ResourceInfo* ()>;

    struct ImporterEntry {
        ResourceImportFunction importFunction;
        ResourceImportUiFunction uiFunction;
		ResourceInfoDefaultConstructor infoConstructor;
    };

    class ResourceImporter {
    public:
        Shared<Resource> import(ResourceManager& rm, const std::filesystem::path& filePath,
            ResourceInfo* resourceInfo = nullptr);

        ImporterEntry* getImporterEntry(const std::string& extension);

	private:
        std::unordered_map<std::string, ImporterEntry> m_extensionToImporterHandler = {
            {".png", {TextureImporter::import, TextureImporter::updateUi, []() { return new ImageInfo(); }}},
            {".jpg", {TextureImporter::import, TextureImporter::updateUi, []() { return new ImageInfo(); }}},
            {".jpeg", {TextureImporter::import, TextureImporter::updateUi, []() { return new ImageInfo(); }}},
            {".obj", {MeshImporter::importObj, MeshImporter::updateUi, []() { return new MeshInfo(); }}}
        };
    };
}