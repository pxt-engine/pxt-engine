#include "resources/resource_manager.hpp"
#include "ui/widgets/space.hpp"

namespace pxt {

    Shared<Material> ResourceManager::s_defaultMaterial = nullptr;
    Shared<Mesh> ResourceManager::s_defaultObjMesh = nullptr;

    ResourceManager::ResourceManager() : Layer("ResourceManager") {}

    ResourceManager::~ResourceManager() {
        s_defaultMaterial = nullptr;
        s_defaultObjMesh = nullptr;
    }

    const std::vector<Shared<Resource>> ResourceManager::getResourcesByType(Resource::Type type) const {
        std::vector<Shared<Resource>> resourcesOfType;
        for (const auto& [uuid, resource] : m_resources) {
            if (resource->getType() == type) {
                resourcesOfType.push_back(resource);
            }
        }
        return resourcesOfType;
    }

    Shared<Resource> ResourceManager::get(const core::UUID uuid, [[maybe_unused]] ResourceInfo* resourceInfo) {
        const auto it = m_resources.find(uuid);
        if (it != m_resources.end()) {
            return it->second;
        }

        PXT_ERROR("Resource {} not found! this should be impossible!", uuid.toString());

        // TODO: use std::optional
        return nullptr;
    }

    Shared<Resource> ResourceManager::get(const std::string& alias, ResourceInfo* resourceInfo) {

        auto aliasIt = m_aliases.find(alias);

        const ResourceId id = aliasIt != m_aliases.end() ? aliasIt->second    // Retrieve the ID from the alias map.
                                                         : ResourceId(alias); // Try using the alias as a UUID string.

        const auto it = m_resources.find(id);
        if (it != m_resources.end()) {
            return it->second;
        }

        const auto filePath = std::filesystem::path(alias);

        try {
            auto importedResource = m_resourceImporter.import(filePath, resourceInfo);

            add(importedResource, alias);

            return importedResource;
        } catch (const std::exception& e) {
            std::cerr << "Failed to import resource: " << e.what() << '\n';
            return nullptr;
        }
    }

    ResourceId ResourceManager::add(const Shared<Resource>& resource, const std::string& alias) {
        const ResourceId id = resource->id;
        m_resources[id] = resource;
        m_aliases[alias] = id;

        resource->alias = alias;

        return id;
    }

    void ResourceManager::foreach (const std::function<void(const Shared<Resource>&)>& function) {
        for (const auto& resource : m_resources | std::views::values) {
            function(resource);
        }
    }

    void ResourceManager::onUpdateUi([[maybe_unused]] FrameInfo& frameInfo) {
        // probably call ui code for imports and asset browser?
        // Asset browser could be a "view" class of all resources given
        // by the resource manager

        // create import window
        ImGui::Begin("Import");

        if (ImGui::Button("Select File")) {
            m_currentlyImportingResourcePath = core::FileSystem::openFileDialog();

            if (!m_currentlyImportingResourcePath.empty()) {
                std::string extension =
                    m_currentlyImportingResourcePath.substr(m_currentlyImportingResourcePath.find_last_of('.'));

                ImporterEntry* entry = m_resourceImporter.getImporterEntry(extension);
                if (!entry) { // for now only error is unsupported format
                    // TODO: use a proper error handling mechanism, like a Result<T> type
                    core::FileSystem::openErrorModal("Unsupported file format: " + extension);
                } else {
                    m_currentImporterEntry = entry;
                    m_currentImportResourceInfo.reset(m_currentImporterEntry->infoConstructor());
                    m_isImportingResource = true;
                }
            }
        }

        if (m_isImportingResource) {
            // ui stuff
            ImGui::Text("Importing: %s", m_currentlyImportingResourcePath.c_str());

            m_currentImporterEntry->uiFunction(m_currentImportResourceInfo.get());

            ui::Space::render(0.0, 15.0);

            if (ImGui::Button("Import")) {
                // try catch may be unnecessary in the future
                // we dont know yet if we will have exceptions here
                // or only ui error messages.
                try {
                    std::string filename = m_currentlyImportingResourcePath.substr(
                        m_currentlyImportingResourcePath.find_last_of("/\\") + 1);

                    auto importedResource =
                        m_resourceImporter.import(m_currentlyImportingResourcePath, m_currentImportResourceInfo.get());

                    if (!importedResource) {
                        m_isImportingResource = false;
                        return;
                    }

                    importedResource->alias = filename;
                    add(importedResource, importedResource->alias);

                    m_isImportingResource = false;

                    PXT_INFO("Imported resource: {}\n", importedResource->alias.c_str());
                } catch (const std::exception& e) {
                    m_isImportingResource = false;
                    PXT_ERROR("Failed to import resource: {}\n", e.what());
                }
            }
        }

        ImGui::End();
    }
} // namespace pxt