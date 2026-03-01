#include "graphics/descriptors/descriptor_writer.hpp"

namespace pxt {
    DescriptorWriter::DescriptorWriter(Context& context)
        : m_context(context) {}

    void DescriptorWriter::updateAll() {
        if (m_writes.empty()) {
            return;
        }

        vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0,
                               nullptr);
    }

} // namespace pxt