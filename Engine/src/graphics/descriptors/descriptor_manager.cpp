#include "graphics/descriptors/descriptor_manager.hpp"

namespace pxt {
	DescriptorManager::DescriptorManager(Context& context) : m_context(context) {}

	void DescriptorManager::createDescriptorPoolAllocator(float textureRatio) {
        // for now we have one ubo and a lot of textures
        std::vector<PoolSizeRatio> ratios = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureRatio},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.0f},
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 2.0f}};

        m_descriptorPoolAllocator =
            createUnique<DescriptorAllocatorGrowable>(m_context, SwapChain::MAX_FRAMES_IN_FLIGHT, ratios);
    }

    void DescriptorManager::initializeManagedSetBindingInfo(DescriptorEntry entry, ManagedDescriptorSet& managedSet) {
        auto& bindingInfo = managedSet.bindingInfos[entry.binding];
        
        switch (entry.descriptorType) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:                
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: 
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            bindingInfo.elements = std::vector<VkDescriptorImageInfo>(entry.count);
            break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:         
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: 
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: 
            bindingInfo.elements = std::vector<VkDescriptorBufferInfo>(entry.count);
            break;

        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            bindingInfo.elements = std::vector<VkWriteDescriptorSetAccelerationStructureKHR>(entry.count);
            break;
        
        // TODO: add texel buffers if we ever need them

        default:
            PXT_ASSERT(false, "Unsupported descriptor type in DescriptorManager::initializeManagedSetBindingInfo!");
            break;
        }
    }

    DescriptorSetHandle DescriptorManager::allocateOneSetPerFrame(VkDescriptorSetLayout rawLayout, ManagedDescriptorSet newSet) {
        // allocate the descriptor sets for each frame in flight
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
            m_descriptorPoolAllocator->allocate(rawLayout, descriptorSet);
            newSet.descriptorSets[i] = descriptorSet;
        }

        // generate a unique handle for the descriptor set and store it
        DescriptorSetHandle handle = core::UID();
        m_managedDescriptorSets[handle] = std::move(newSet);

        return handle;
    }

    DescriptorSetHandle DescriptorManager::createSet(std::vector<DescriptorEntry> bindingsDescriptions) {
        ManagedDescriptorSet managedSet;

        DescriptorSetLayout::Builder layoutBuilder(m_context);

        for (const auto& entry : bindingsDescriptions) {
            layoutBuilder.addBinding(entry.binding, entry.descriptorType, entry.stageFlags, entry.count);

            initializeManagedSetBindingInfo(entry, managedSet);
        }

        Shared<DescriptorSetLayout> layout = layoutBuilder.build();
        VkDescriptorSetLayout rawLayout = layout->getHandle();

        managedSet.layout = layout;

        return allocateOneSetPerFrame(rawLayout, std::move(managedSet));
	}

    DescriptorSetHandle DescriptorManager::createSet(Shared<DescriptorSetLayout> setLayout) {
        ManagedDescriptorSet managedSet;

        // extract entries from layout
        std::vector<DescriptorEntry> entriesFromLayout = setLayout->buildDescriptorEntries();

        for (const auto& entry : entriesFromLayout) {
            initializeManagedSetBindingInfo(entry, managedSet);
        }

        VkDescriptorSetLayout rawLayout = setLayout->getHandle();

        // here we can move the layout
        managedSet.layout = setLayout;

        return allocateOneSetPerFrame(rawLayout, std::move(managedSet));
    }

    DescriptorSetHandle DescriptorManager::createSet(DescriptorSetHandle setToCopyLayoutFrom) {
        ManagedDescriptorSet managedSet;

        Shared<DescriptorSetLayout> setLayout = m_managedDescriptorSets.at(setToCopyLayoutFrom).layout;

        // extract entries from layout
        std::vector<DescriptorEntry> entriesFromLayout = setLayout->buildDescriptorEntries();

        for (const auto& entry : entriesFromLayout) {
            initializeManagedSetBindingInfo(entry, managedSet);
        }

        VkDescriptorSetLayout rawLayout = setLayout->getHandle();

        // here we can move the layout
        managedSet.layout = setLayout;

        return allocateOneSetPerFrame(rawLayout, std::move(managedSet));
    }

    void DescriptorManager::writeSetPendingUpdates(uint32_t frameIndex, ManagedDescriptorSet& managedSet, DescriptorWriter& descriptorWriter) {
        for (auto& [bindingSlot, bindingInfo] : managedSet.bindingInfos) {
            // build the write
            std::visit(
                [&](auto& vec) {
                    using DescriptorInfoArrayType = std::decay_t<decltype(vec)>;

                    if constexpr (!std::is_same_v<DescriptorInfoArrayType, std::monostate>) {
                        descriptorWriter.write(*managedSet.layout, managedSet.descriptorSets[frameIndex], bindingSlot,
                                               vec.data(), static_cast<uint32_t>(vec.size()));
                    }
                },
                bindingInfo.elements);
        }
    }

    void DescriptorManager::flushUpdates(uint32_t frameIndex) {
        DescriptorWriter descriptorWriter(m_context);

        for (auto& [handle, managedSet] : m_managedDescriptorSets) {
            // this frame set does not need an update
            if (!managedSet.dirtyFlags.at(frameIndex)) {
                continue;
            }

            writeSetPendingUpdates(frameIndex, managedSet, descriptorWriter);

            managedSet.dirtyFlags.at(frameIndex) = false;
        }

        descriptorWriter.updateAll();
    }

    void DescriptorManager::flushUpdatesForSet(DescriptorSetHandle setHandle, uint32_t frameIndex) {
        DescriptorWriter descriptorWriter(m_context);
        ManagedDescriptorSet& setToUpdate = m_managedDescriptorSets[setHandle];
        
        // this frame set does not need an update
        if (!setToUpdate.dirtyFlags.at(frameIndex)) {
            return;
        }

        writeSetPendingUpdates(frameIndex, setToUpdate, descriptorWriter);

        setToUpdate.dirtyFlags.at(frameIndex) = false;
        
        descriptorWriter.updateAll();
    }
	
} // namespace pxt