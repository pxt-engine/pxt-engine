#pragma once

#include "tinyfiledialogs.h"

namespace pxt::core {
    class FileSystem {
    public:
        static std::string openFileDialog();
        static void openErrorModal(const std::string& message);
        static const std::vector<std::string> getAllFilesRecursive(const std::string& directory, bool relative);
    };
} // namespace pxt::core