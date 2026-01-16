#pragma once

#include "pxtengine.h"
#include "ui/widgets/space.hpp"

namespace pxt::editor {

    template <typename Component>
    using ComponentUiFunction = std::function<void(Component&, Entity entity)>;

    struct ComponentUiInfo {
        std::string name;
        bool essential;
        // the function that will draw the ImGui component
        std::function<void(Entity)> drawer;
        std::function<void(Entity)> addComponent;
    };

    class EntityInspector {
    public:
        EntityInspector();
        ~EntityInspector();

        EntityInspector(const EntityInspector&) = delete;
        EntityInspector& operator=(const EntityInspector&) = delete;

        void registerComponents();

        void drawEntityInspector(Scene& scene, const core::UUID& selectedEntityId);
        void onUpdateUi(FrameInfo& frameInfo, const core::UUID& selectedEntityId);

    private:
        std::vector<ComponentUiInfo> m_componentUiRegistry;

        bool m_openAddComponentWindow = false;

        /*
         * @brief Factory function that generates an "adder" callback for a specific component type.
         *
         * This is used by the editor entity inspector component registry to determine how (or if) a component
         * can be added to an entity from the "Add Component" UI.
         *
         * - If the component is marked as essential, the returned function is a no-op,
         *   because essential components are created automatically and must not be added
         *   manually through the UI.
         *
         * - If the component is not essential, this function enforces at compile time that
         *   the component is default-constructible, since the UI has no way to supply
         *   constructor arguments for now.
         *
         * The use of `if constexpr` is critical: it ensures that invalid code paths
         * (such as calling `entity.add<Component>()` for non-default-constructible
         * components) are never instantiated by the compiler.
         */
        template <typename Component>
        static std::function<void(Entity)> MakeAdder(std::string compName) {
            if constexpr (IsCoreComponent<Component>::value) {
                return [](Entity) {};
            } else {
                PXT_STATIC_ASSERT(std::is_default_constructible_v<Component>,
                                  "Component must be default constructible to be added from the UI");
                return [compName](Entity entity) {
                    // TODO: in the future script components will need extra config care
                    if (!entity.has<Component>()) {
                        entity.add<Component>();

                        PXT_INFO("Added {} to Entity \"{}\"", compName, entity.getName());
                    } else {
                        std::string message = "Entity (" + entity.getName() + ") already has " + compName + "!";

                        core::FileSystem::openWarningModal(message);
                    }
                };
            }
        }

        /*
         *@brief Registers a component of type T into the m_componentUiRegistry.
         *		 Each element has a name and a function that dictates how it is
         *		 drawn into the entity inspector drawer.
         *
         */
        template <typename Component>
        void RegisterComponent(const std::string& name, ComponentUiFunction<Component> uiFunction) {
            m_componentUiRegistry.push_back(
                {name, IsCoreComponent<Component>::value,
                 [=](pxt::Entity entity) {
                     if (entity.has<Component>()) {
                         Component& component = entity.get<Component>();

                         //? maybe also defualt open flag?
                         ImGuiTreeNodeFlags treeFlags =
                             ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

                         // here we render without the close button for necessary components
                         if constexpr (IsCoreComponent<Component>::value) {
                             if (ImGui::CollapsingHeader(name.c_str(), treeFlags)) {
                                 uiFunction(component, entity);
                                 ui::Space::render(0.0f, 5.0f);
                             }

                             return;
                         }

                         bool hasComponent = true;
                         if (ImGui::CollapsingHeader(name.c_str(), &hasComponent, treeFlags)) {
                             uiFunction(component, entity);
                             ui::Space::render(0.0f, 5.0f);
                         }

                         //! close button on header, we need to remove this component
                         if constexpr (!IsCoreComponent<Component>::value) {
                             if (!hasComponent) {
                                 entity.remove<Component>();
                                 PXT_INFO("Removed {} from Entity \"{}\"", name, entity.getName());
                             }
                         }
                     }
                 },
                 MakeAdder<Component>(name)});
        }
    };
} // namespace pxt::editor