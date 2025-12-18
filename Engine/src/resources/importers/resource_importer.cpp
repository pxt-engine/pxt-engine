#include "resources/importers/resource_importer.hpp"
#include "resources/resource.hpp"

namespace pxt {
    ImporterEntry* ResourceImporter::getImporterEntry(const std::string& extension) {
        auto it = m_extensionToImporterHandler.find(extension);
        if (it != m_extensionToImporterHandler.end()) {
            return &it->second;
        }
        return nullptr;
    }

    Shared<Resource> ResourceImporter::import(ResourceManager& rm, const std::filesystem::path& filePath,
                                              ResourceInfo* resourceInfo) {

        std::string extension = filePath.extension().string();

        ImporterEntry* entry = getImporterEntry(extension);

        if (!entry) {
            core::FileSystem::openErrorModal(
                "Unsupported file format: " + extension +
                " (THIS SHOULD NOT BE CALLED, ResourceManager should handle it before import");
            // TODO: use a proper error handling mechanism, like a Result<T> type
            return nullptr;
        }

        return entry->importFunction(rm, filePath, resourceInfo);
    }
} // namespace pxt