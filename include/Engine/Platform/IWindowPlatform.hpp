#pragma once

#include <filesystem>
#include <string>
#include <vector>

class IWindowPlatform
{
public:
    struct DialogFilter
    {
        std::string name;
        std::string pattern;
    };

    virtual ~IWindowPlatform() = default;

    virtual std::filesystem::path openFileDialog(const std::string& title, const std::vector<DialogFilter>& filter) = 0;

    virtual std::filesystem::path openFolderDialog(const std::string& title) = 0;

    virtual void recycleToTrash(const std::filesystem::path& path) = 0;

    virtual void openPath(const std::string& path) = 0;
};
