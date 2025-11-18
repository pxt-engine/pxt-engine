#include "resources/resource_manager.hpp"

namespace PXTEngine {

	Shared<Material> ResourceManager::defaultMaterial = nullptr;

	ResourceManager::ResourceManager() : Layer("ResourceManager") {}

	ResourceManager::~ResourceManager() {
		defaultMaterial = nullptr;
	}

	Shared<Resource> ResourceManager::get(const std::string& alias, ResourceInfo* resourceInfo) {

		auto aliasIt = m_aliases.find(alias);

		const ResourceId id = aliasIt != m_aliases.end()
			? aliasIt->second    // Retrieve the ID from the alias map.
			: ResourceId(alias); // Try using the alias as a UUID string.

		const auto it = m_resources.find(id);
		if (it != m_resources.end()) {
			return it->second;
		}

		const auto filePath = std::filesystem::path(alias);

		try {
			auto importedResource = m_resourceImporter.import(*this, filePath, resourceInfo);

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

	void ResourceManager::foreach(const std::function<void(const Shared<Resource>&)>& function) {
		for (const auto& resource : m_resources | std::views::values) {
			function(resource);
		}
	}

	void ResourceManager::onUpdateUi(FrameInfo& frameInfo) {
		// probably call ui code for imports and asset browser?
		// Asset browser could be a "view" class of all resources given
		// by the resource manager
		
		// create import window
		ImGui::Begin("Import");

		if (ImGui::Button("Select File")) {
			m_currentlyImportingResourcePath = FileSystem::openFileDialog();

			if (!m_currentlyImportingResourcePath.empty()) {
				m_isImportingResource = true;
			}
		}

		if (m_isImportingResource) {
			// ui stuff
			ImGui::Text("Importing: %s", m_currentlyImportingResourcePath.c_str());

			if (ImGui::Button("Import")) {
				try {
					std::string filename = m_currentlyImportingResourcePath.substr(
						m_currentlyImportingResourcePath.find_last_of("/\\") + 1);

					auto importedResource = m_resourceImporter.import(*this, m_currentlyImportingResourcePath);
					importedResource->alias = filename;
					add(importedResource, importedResource->alias);
					PXT_INFO("Imported resource: {}\n", importedResource->alias.c_str());
				}
				catch (const std::exception& e) {
					PXT_ERROR("Failed to import resource: {}\n", e.what());
				}

				m_isImportingResource = false;
			}
		}

		ImGui::End();
	}
}