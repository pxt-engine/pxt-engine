#pragma once

#include "core/pch.hpp"
#include "core/uid.hpp"

namespace pxt {
    // UID wrapper for asset management
    struct AssetHandle {
        core::UID uid = core::UID::s_invalidId;

        AssetHandle() = default;

        AssetHandle(core::UID uid) : uid(uid) {}

        bool isValid() const { return uid != core::UID::s_invalidId; }

        bool operator==(const AssetHandle& other) const { return uid == other.uid; }

        operator uint64_t() const { return uid; }

        operator core::UID() const { return uid; }
    };
} // namespace pxt