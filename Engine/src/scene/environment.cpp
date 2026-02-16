#include "scene/environment.hpp"

#include "graphics/resources/vk_skybox.hpp"

namespace pxt {

    void Environment::setSkybox(const std::array<std::string, 6>& skyboxTextures, uint32_t frameIndex) {
        if (m_skybox) {
            m_skybox->replace(skyboxTextures, frameIndex);
        } else {
            m_skybox = VulkanSkybox::create(skyboxTextures);
        }
    }
} // namespace pxt