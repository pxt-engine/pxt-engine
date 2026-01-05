#include "core/filesystem.hpp"

#include "tinyfiledialogs.h"

namespace pxt::core {
    std::string FileSystem::openFileDialog() {
        const char* path = tinyfd_openFileDialog("Select File to Import",
                                                 "",      // default path / file
                                                 0,       // num filters
                                                 nullptr, // filters
                                                 nullptr,
                                                 0 // allow multiple select
        );

        if (path) {
            std::string fullPath(path);

            return fullPath;
        }

        return "";
    }

    void FileSystem::openErrorModal(const std::string& message) {
        tinyfd_messageBox("Error", message.c_str(), "ok", "error", 1);
    }

    const std::vector<std::string> FileSystem::getAllFilesRecursive(const std::string& directory, bool relative) {
        namespace fs = std::filesystem;

        std::vector<std::string> result;

        fs::path rootPath(directory);

        if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
            return result; // empty on invalid path
        }

        for (const fs::directory_entry& entry :
             fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file()) {
                if (!relative) {
                    result.emplace_back(entry.path().string());
                    continue;
                }

                const std::string& filename = fs::relative(entry.path(), rootPath).string();
                result.emplace_back(filename);
            }
        }

        return result;
    }
} // namespace pxt::core