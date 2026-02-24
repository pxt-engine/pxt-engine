#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/swap_chain.hpp"
#include "core/uid.hpp"

namespace pxt {
    /*
     * @brief A variant type that can hold any of the descriptor info types used for updating descriptor sets.
	*/
    using DescriptorInfoVariant = std::variant<std::monostate, VkDescriptorImageInfo, VkDescriptorBufferInfo, VkWriteDescriptorSetAccelerationStructureKHR>;
    
	/*
	 * @brief A variant type that can hold vectors of descriptor info types for array updates.
     */
	using DescriptorInfoArrayVariant = std::variant<
		std::vector<VkDescriptorImageInfo>,
		std::vector<VkDescriptorBufferInfo>,
		std::vector<VkWriteDescriptorSetAccelerationStructureKHR>>;

	struct BindingInfo {
		uint32_t count;
        DescriptorInfoArrayVariant elements; // size should be equal to count
    };

	struct ManagedDescriptorSet {
        std::array<VkDescriptorSet, SwapChain::MAX_FRAMES_IN_FLIGHT> descriptorSets{VK_NULL_HANDLE};
        
		Shared<DescriptorSetLayout> layout;

		// Tracking which frames need an update
        // index 0 = frame 0, index 1 = frame 1, etc.
		//! do not use vector<bool>!!! it is less efficient and more error prone
        std::array<bool, SwapChain::MAX_FRAMES_IN_FLIGHT> dirtyFlags{false};

        // The current descriptor infos of what is in this set
		// this gets filled when callers call "update" on a set binding
        // Mapping: Binding Index -> Resource Infos
        std::unordered_map<uint32_t, BindingInfo> bindingInfos;
    };

	using DescriptorSetHandle = core::UID;

	class DescriptorManager {
	  public:
		explicit DescriptorManager(Context& context);
        
        /*
         *@brief Creates one set per frame in flight given the bindings descriptions.
         * 
         *@param bindingsDescriptions A vector of DescriptorEntry that describes each binding of the set
         * 
         *@return The DescriptorSetHandle of the created sets.
         */
        [[nodiscard]] DescriptorSetHandle createSet(std::vector<DescriptorEntry> bindingsDescriptions);

        // overload to take a layout instead of the wrapper DescriptorEntry vector.
        [[nodiscard]] DescriptorSetHandle createSet(Shared<DescriptorSetLayout> setLayout);

        // overload to copy the layout of an existing set.
        [[nodiscard]] DescriptorSetHandle createSet(DescriptorSetHandle setToCopyLayoutFrom);
 
	    const DescriptorSetLayout& getLayout(DescriptorSetHandle handle) const {
			return *m_managedDescriptorSets.at(handle).layout;
        }

        const VkDescriptorSetLayout& getRawLayout(DescriptorSetHandle handle) const {
            return m_managedDescriptorSets.at(handle).layout->getHandle();
        }

		const VkDescriptorSet getDescriptorSet(DescriptorSetHandle handle, uint32_t frameIndex) const {
			return m_managedDescriptorSets.at(handle).descriptorSets[frameIndex];
        }

        const VkDescriptorSet* getDescriptorSetPtr(DescriptorSetHandle handle, uint32_t frameIndex) const {
            return &(m_managedDescriptorSets.at(handle).descriptorSets[frameIndex]);
        }
        
        /*
         * @brief Submits an update for an array of descriptors in a descriptor set.
         * 
         * @tparam DescriptorInfoT The type of descriptor info (e.g., VkDescriptorImageInfo,
         * VkDescriptorBufferInfo).
         * @param handle The handle of the descriptor set to update.
         * @param binding The binding index to update.
         * @param infos A span of descriptor info structures to update the binding with. The size of the span
         * must match the count specified in the descriptor set layout for this binding.
         */
		template <typename DescriptorInfoT>
        void submitUpdateArray(DescriptorSetHandle handle, uint32_t binding,
                                std::span<const DescriptorInfoT> infos) {
            auto& set = m_managedDescriptorSets.at(handle);
            auto& bindingInfo = set.bindingInfos.at(binding);

            std::visit(
                [&](auto& vec) {
                    using VecT = std::decay_t<decltype(vec)>;
                    using ValueT = typename VecT::value_type;

                    //! this decay is essential for the comparison of types, because the vector holds non-reference
                    //! types, but the function can be called with lvalue references, rvalue references, or const
                    //! references. By decaying DescriptorInfoT, we remove references and const qualifiers, allowing for
                    //! a proper type comparison.
                    using DescriptorInfoRawType = std::decay_t<DescriptorInfoT>;

                    if constexpr (std::is_same_v<ValueT, DescriptorInfoRawType>) {
                        PXT_ASSERT(infos.size() == vec.size(), "Descriptor array size mismatch");
                        std::copy(infos.begin(), infos.end(), vec.begin());
                    } else {
                        PXT_ASSERT(false, "Descriptor type does not match binding descriptor type");
                    }
                },
                bindingInfo.elements);

            set.dirtyFlags.fill(true);
        }

        // overload to take vector directly
        template <typename DescriptorInfoT>
        void submitUpdateArray(DescriptorSetHandle handle, uint32_t binding,
                                const std::vector<DescriptorInfoT>& infos) {
            submitUpdateArray(handle, binding, std::span<const DescriptorInfoT>(infos));
        }

        /*
         * @brief Submits an update for a single descriptor in a descriptor set array binding.
         * 
         * @tparam DescriptorInfoT The type of descriptor info (e.g., VkDescriptorImageInfo,
         * @param handle The handle of the descriptor set to update.
         * @param binding The binding index to update.
         * @param info The descriptor info structure to update the binding with.
         * @param arrayIndex The index within the array binding to update (default is 0). Must be less than the count
         * specified in the descriptor set layout for this binding.
         */
        template <typename DescriptorInfoT>
        void submitUpdateSingle(DescriptorSetHandle handle, uint32_t binding,
                                DescriptorInfoT&& info, uint32_t arrayIndex = 0) {
            auto& set = m_managedDescriptorSets.at(handle);
            auto& bindingInfo = set.bindingInfos.at(binding);

            std::visit(
                [&](auto& vec) {
                    using VecT = std::decay_t<decltype(vec)>;
                    using ValueT = typename VecT::value_type;

                    //! this decay is essential for the comparison of types, because the vector holds non-reference
                    //! types, but the function can be called with lvalue references, rvalue references, or const
                    //! references. By decaying DescriptorInfoT, we remove references and const qualifiers, allowing for
                    //! a proper type comparison.
                    using DescriptorInfoRawType = std::decay_t<DescriptorInfoT>;

                    if constexpr (std::is_same_v<ValueT, DescriptorInfoRawType>) {
                        PXT_ASSERT(arrayIndex < vec.size(), "Array index out of bounds for descriptor binding");
                        
                        // Use forward to preserve lvalue/rvalue status
                        vec[arrayIndex] = std::forward<DescriptorInfoT>(info);
                    } else {
                        PXT_ASSERT(false, "Descriptor type does not match binding descriptor type");
                    }
                },
                bindingInfo.elements);

            set.dirtyFlags.fill(true);
        }

        void flushUpdatesForSet(DescriptorSetHandle setHandle, uint32_t frameIndex);

		void flushUpdates(uint32_t frameIndex);

      protected:
        void createDescriptorPoolAllocator(float textureRatio);
	  private:
        void initializeManagedSetBindingInfo(DescriptorEntry entry, ManagedDescriptorSet& managedSet);

        void writeSetPendingUpdates(uint32_t frameIndex, ManagedDescriptorSet& managedSet,
                                                       DescriptorWriter& descriptorWriter);

        /*
         *@brief Creates one set per frame in flight given the raw vulkan layout and the new managed set.
         *
         *@param rawLayout The Vulkan set layout of the set
         *
         *@return The DescriptorSetHandle of the created sets.
         */
        DescriptorSetHandle allocateOneSetPerFrame(VkDescriptorSetLayout rawLayout, ManagedDescriptorSet newSet);

        friend class Application;

		Context& m_context;

		Unique<DescriptorAllocatorGrowable> m_descriptorPoolAllocator = nullptr;

		std::unordered_map<DescriptorSetHandle, ManagedDescriptorSet> m_managedDescriptorSets;
		
		//TODO: descriptor set layout cache
	};
} // namespace pxt