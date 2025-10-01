#include "resources/types/material.hpp"

#include "resources/resource_manager.hpp"

namespace PXTEngine {
    Material::Material(
        const glm::vec4& albedoColor,
        const Shared<Image>& albedoMap,
        const Shared<Image>& normalMap,
		const float metallic,
        const Shared<Image>& metallicMap,
		const float roughness,
        const Shared<Image>& roughnessMap,
        const Shared<Image>& ambientOcclusionMap,
        const glm::vec4& emissiveColor,
        const Shared<Image>& emissiveMap,
		const float transmission,
        const float ior,
        const float blinnPhongSpecularIntensity,
        const float blinnPhongSpecularShininess
    )
        : m_albedoColor(albedoColor),
        m_albedoMap(albedoMap),
        m_normalMap(normalMap),
		m_metallic(metallic),
        m_metallicMap(metallicMap),
		m_roughness(roughness),
        m_roughnessMap(roughnessMap),
        m_ambientOcclusionMap(ambientOcclusionMap),
        m_emissiveColor(emissiveColor),
        m_emissiveMap(emissiveMap),
		m_transmission(transmission),
		m_ior(ior),
        m_blinnPhongSpecularIntensity(blinnPhongSpecularIntensity),
        m_blinnPhongSpecularShininess(blinnPhongSpecularShininess)
        {}

    Material::Type Material::getStaticType() {
        return Type::Material;
    }

    Material::Type Material::getType() const {
        return Type::Material;
    }

    const glm::vec4& Material::getAlbedoColor() const { return m_albedoColor; }
    Shared<Image> Material::getAlbedoMap() const { return m_albedoMap; }
	float Material::getMetallic() const { return m_metallic; }
    Shared<Image> Material::getMetallicMap() const { return m_metallicMap; }
	float Material::getRoughness() const { return m_roughness; }
    Shared<Image> Material::getRoughnessMap() const { return m_roughnessMap; }
    Shared<Image> Material::getNormalMap() const { return m_normalMap; }
    Shared<Image> Material::getAmbientOcclusionMap() const { return m_ambientOcclusionMap; }
    const glm::vec4& Material::getEmissiveColor() const { return m_emissiveColor; }
    Shared<Image> Material::getEmissiveMap() const { return m_emissiveMap; }
	float Material::getTransmission() const { return m_transmission; }
	float Material::getIndexOfRefraction() const { return m_ior; }
    float Material::getBlinnPhongSpecularIntensity() const { return m_blinnPhongSpecularIntensity;}
    float Material::getBlinnPhongSpecularShininess() const { return m_blinnPhongSpecularShininess; }

    bool Material::isEmissive() {
        return m_emissiveColor.a > 0.0f;
    }

    // -------- Builder Implementation --------

    Material::Builder& Material::Builder::setAlbedoColor(const glm::vec4& color) {
        m_albedoColor = color;
        return *this;
    }

    Material::Builder& Material::Builder::setAlbedoMap(Shared<Image> map) {
        m_albedoMap = map;
        return *this;
    }

	Material::Builder& Material::Builder::setMetallic(const float metallic) {
		m_metallic = metallic;
		m_useMetallicWeight = true;
		return *this;
	}

    Material::Builder& Material::Builder::setMetallicMap(Shared<Image> map) {
        m_metallicMap = map;
        return *this;
    }

	Material::Builder& Material::Builder::setRoughness(const float roughness) {
		m_roughness = roughness;
		m_useRoughnessWeight = true;
		return *this;
	}

    Material::Builder& Material::Builder::setRoughnessMap(Shared<Image> map) {
        m_roughnessMap = map;
        return *this;
    }

    Material::Builder& Material::Builder::setNormalMap(Shared<Image> map) {
        m_normalMap = map;
        return *this;
    }

    Material::Builder& Material::Builder::setAmbientOcclusionMap(Shared<Image> map) {
        m_ambientOcclusionMap = map;
        return *this;
    }

    Material::Builder& Material::Builder::setEmissiveColor(const glm::vec4& color) {
        m_emissiveColor = color;
        return *this;
    }

    Material::Builder& Material::Builder::setEmissiveMap(Shared<Image> map) {
        m_emissiveMap = map;
        return *this;
    }

	Material::Builder& Material::Builder::setTransmission(const float transmission) {
		m_transmission = transmission;
		return *this;
	}

	Material::Builder& Material::Builder::setIndexOfRefraction(const float ior) {
		m_ior = ior;
		return *this;
	}

    Material::Builder& Material::Builder::setBlinnPhongSpecularIntensity(float value)
    {
        m_blinnPhongSpecularIntensity = value;
        return *this;
    }

    Material::Builder& Material::Builder::setBlinnPhongSpecularShininess(float value)
    {
        m_blinnPhongSpecularShininess = value;
        return *this;
    }

    Shared<Material> Material::Builder::build() {
        if (!m_albedoMap) m_albedoMap = ResourceManager::defaultMaterial->getAlbedoMap();
        if (!m_normalMap) m_normalMap = ResourceManager::defaultMaterial->getNormalMap();

		m_metallic = m_useMetallicWeight ? m_metallic : 1.0;
		m_roughness = m_useRoughnessWeight ? m_roughness : 1.0;

        //if (!m_metallicMap) m_metallicMap = ResourceManager::defaultMaterial->getMetallicMap();
        //if (!m_roughnessMap) m_roughnessMap = ResourceManager::defaultMaterial->getRoughnessMap();

        if (!m_ambientOcclusionMap) m_ambientOcclusionMap = ResourceManager::defaultMaterial->getAmbientOcclusionMap();
        if (!m_emissiveMap) m_emissiveMap = ResourceManager::defaultMaterial->getEmissiveMap();

        return createShared<Material>(
            m_albedoColor,
            m_albedoMap,
            m_normalMap,
			m_metallic,
            m_metallicMap,
			m_roughness,
            m_roughnessMap,
            m_ambientOcclusionMap,
            m_emissiveColor,
            m_emissiveMap,
            m_transmission,
			m_ior,
            m_blinnPhongSpecularIntensity,
            m_blinnPhongSpecularShininess
        );
    }

	// -------- UI --------
    void Material::drawMaterialUi() {
        ImGui::SliderFloat("Metallic", &m_metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);

        if (ImGui::TreeNode("Textures (Work in progress)")) {
            // TODO: handle texture window on selection
            // either we modify imgui backend to support bindless access
			// or we create a descriptor set per material (i dont like this one)

			if (m_albedoMap) {
				ImGui::Text("Albedo Map is set (id: %s, alias: %s)",
					m_albedoMap->id.toString().c_str(), m_albedoMap->alias.c_str());
			}

			if (m_normalMap) {
				ImGui::Text("Normal Map is set (id: %s, alias: %s)",
					m_normalMap->id.toString().c_str(), m_normalMap->alias.c_str());
			}

			if (m_metallicMap) {
				ImGui::Text("Metallic Map is set (id: %s, alias: %s)",
					m_metallicMap->id.toString().c_str(), m_metallicMap->alias.c_str());
			}

			if (m_roughnessMap) {
				ImGui::Text("Roughness Map is set (id: %s, alias: %s)",
					m_roughnessMap->id.toString().c_str(), m_roughnessMap->alias.c_str());
			}

            if (m_emissiveMap) {
                ImGui::Text("Emissive Map is set (id: %s, alias: %s)",
                    m_emissiveMap->id.toString().c_str(), m_emissiveMap->alias.c_str());
            }

			ImGui::TreePop();
        }

		ImGui::ColorEdit3("Emissive Color", glm::value_ptr(m_emissiveColor));
		ImGui::SliderFloat("Emissive Intensity", &m_emissiveColor.a, 0.0f, 100.0f);

		ImGui::SliderFloat("Transmission", &m_transmission, 0.0f, 1.0f);
		ImGui::SliderFloat("Index of Refraction", &m_ior, 1.0f, 3.0f);

        if (ImGui::TreeNode("Blinn-Phong Specular Parameters")) {
            ImGui::SliderFloat("Specular Intensity", &m_blinnPhongSpecularIntensity, 0.0f, 1.0f);
            ImGui::SliderFloat("Specular Shininess", &m_blinnPhongSpecularShininess, 1.0f, 50.0f);

            ImGui::TreePop();
        }   
    }
}