#include "Engine/FileExplorer.hpp"
#include "Engine/Platform/PlatformServices.hpp"
#include "Engine/Log.hpp"

#include <memory>

namespace
{
std::vector<IWindowPlatform::DialogFilter> toPlatformFilter(
    const std::vector<std::pair<std::string, std::string>>& filters)
{
    std::vector<IWindowPlatform::DialogFilter> result;
    result.reserve(filters.size());
    for (const auto& filter : filters)
    {
        result.push_back({filter.first, filter.second});
    }
    return result;
}
} // namespace

std::filesystem::path openFolderExplorerAndGetAbsoluePath(const std::string& title)
{
    auto windowPlatform = PlatformServices::createWindowPlatform();
    return windowPlatform->openFolderDialog(title);
}

std::filesystem::path openFolderExplorerAndGetRelativePath(const std::string& title)
{
    return std::filesystem::relative(openFolderExplorerAndGetAbsoluePath(title));
}

std::filesystem::path openFileExplorerAndGetAbsoluePath(
    const std::string&                              title,
    const std::vector<std::pair<std::string, std::string>>& filter)
{
    auto windowPlatform = PlatformServices::createWindowPlatform();
    return windowPlatform->openFileDialog(title, toPlatformFilter(filter));
}

std::filesystem::path openFileExplorerAndGetRelativePath(const std::string&                              title,
                                                        const std::vector<std::pair<std::string, std::string>>& filter)
{
    return std::filesystem::relative(openFileExplorerAndGetAbsoluePath(title, filter));
}

void recycleFileOrDirectory(const std::filesystem::path& path)
{
    auto windowPlatform = PlatformServices::createWindowPlatform();
    windowPlatform->recycleToTrash(path);
}

void SystemOpen(const std::string& path)
{
    auto windowPlatform = PlatformServices::createWindowPlatform();
    if (!windowPlatform)
    {
        logf("Failed to initialize the platform window service");
        return;
    }
    windowPlatform->openPath(path);
}
