#pragma once

#include "core/pch.hpp"
#include "resources/resource.hpp"
#include "resources/types/image.hpp"

namespace PXTEngine {

	/**
	 * @class Material
	 *
	 * @brief Represents a material resource used for rendering.
	 * This class encapsulates various properties of a material, including color, textures,
	 * and other parameters.
	 */
    class Material : public Resource {
    public:
        class Builder {
        public:
            Builder& setAlbedoColor(const glm::vec4& color);
            Builder& setAlbedoMap(Shared<Image> map);
			Builder& setMetallic(float value);
            Builder& setMetallicMap(Shared<Image> map);
			Builder& setRoughness(float value);
            Builder& setRoughnessMap(Shared<Image> map);
            Builder& setNormalMap(Shared<Image> map);
            Builder& setAmbientOcclusionMap(Shared<Image> map);
            Builder& setEmissiveColor(const glm::vec4& color);
            Builder& setEmissiveMap(Shared<Image> map);
			Builder& setTransmission(float value);
			Builder& setIndexOfRefraction(float value);
            Shared<Material> build();

        protected:
            glm::vec4 m_albedoColor{ 1.0f };
            Shared<Image> m_albedoMap{ nullptr };
            Shared<Image> m_normalMap{ nullptr };
			float m_metallic = 0.0f;
            Shared<Image> m_metallicMap{ nullptr };
			float m_roughness = 0.0f;
            Shared<Image> m_roughnessMap{ nullptr };
            Shared<Image> m_ambientOcclusionMap{ nullptr };
            glm::vec4 m_emissiveColor{ 0.0f };
            Shared<Image> m_emissiveMap{ nullptr };
            float m_transmission = 0.0;
            float m_ior = 1.3;
        private:
			bool m_useMetallicWeight = false;
			bool m_useRoughnessWeight = false;
        };

        Material(
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
			const float ior
        );

        static Type getStaticType();
        Type getType() const override;

        const glm::vec4& getAlbedoColor() const;
        Shared<Image> getAlbedoMap() const;
		float getMetallic() const;
        Shared<Image> getMetallicMap() const;
		float getRoughness() const;
        Shared<Image> getRoughnessMap() const;
        Shared<Image> getNormalMap() const;
        Shared<Image> getAmbientOcclusionMap() const;
        const glm::vec4& getEmissiveColor() const;
        Shared<Image> getEmissiveMap() const;
		float getTransmission() const;
		float getIndexOfRefraction() const;

        /*void setMetallic(float metallic) { m_metallic = metallic; }
		void setRoughness(float roughness) { m_roughness = roughness; }
		void setEmissiveColor(const glm::vec4& color) { m_emissiveColor = color; }
		void setTransmission(float transmission) { m_transmission = transmission; }
		void setIndexOfRefraction(float ior) { m_ior = ior; }*/

        bool isEmissive();

        void drawMaterialUi();

    protected:
        glm::vec4 m_albedoColor{ 1.0f };
        Shared<Image> m_albedoMap{ nullptr };
        Shared<Image> m_normalMap{ nullptr };
		float m_metallic{ 0.0f };
        Shared<Image> m_metallicMap{ nullptr };
		float m_roughness{ 0.0f };
        Shared<Image> m_roughnessMap{ nullptr };
        Shared<Image> m_ambientOcclusionMap{ nullptr };
        glm::vec4 m_emissiveColor{ 0.0f };
        Shared<Image> m_emissiveMap{ nullptr };
		float m_transmission{ 0.0f };
		float m_ior{ 1.3f };
    };
}