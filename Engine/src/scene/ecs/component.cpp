#include "scene/ecs/component.hpp"

#include "application.hpp"

namespace pxt {
    // --- MaterialComponent ---
    MaterialComponent::MaterialComponent() : tilingFactor(1.0f), tint(1.0f) {
        auto rm = Application::get().getResourceManager();
        material = rm->get<Material>(DEFAULT_MATERIAL);
    }

    // --- Transform2dComponent ---
    glm::mat2 Transform2dComponent::mat2() const {
        const float sin = glm::sin(rotation);
        const float cos = glm::cos(rotation);

        glm::mat2 rotationMatrix(cos, sin, -sin, cos);
        glm::mat2 scaleMatrix(scale.x, 0.f, 0.f, scale.y);

        return rotationMatrix * scaleMatrix;
    }

    // --- TransformComponent ---
    glm::mat4 TransformComponent::mat4() const {
        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), translation);

        // create rotation matrix (using quaternions to avoid Gimbal Lock)
        glm::mat4 rotationMat = glm::toMat4(glm::quat(rotation));

        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);

        // T * R * S
        return translationMat * rotationMat * scaleMat;
    }

    glm::mat3 TransformComponent::normalMatrix(const glm::mat4& modelMatrix) const {
        // we only need the top-left 3x3 for normals
        // (when casting to mat3, glm automatically takes the top-left 3x3 by value)
        return glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
    }

    MeshComponent::MeshComponent() { mesh = ResourceManager::s_defaultObjMesh; }

    // --- CameraComponent ---
    CameraComponent::CameraComponent() { cameraData = CameraData(); }
} // namespace pxt