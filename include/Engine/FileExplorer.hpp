#pragma once

#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief Opens a system file picker and returns the selected path.
 *
 * The dialog filters are name/pattern pairs, for example:
 * { "Images", "*.png;*.jpg" }.
 */
std::filesystem::path openFileExplorerAndGetAbsoluePath(const std::string& title  = "Open",
                                                       const std::vector<std::pair<std::string, std::string>>& filter =
                                                           {{"All", "*.*"}});

std::filesystem::path openFileExplorerAndGetRelativePath(const std::string& title  = "Open",
                                                        const std::vector<std::pair<std::string, std::string>>& filter =
                                                            {{"All", "*.*"}});

std::filesystem::path openFolderExplorerAndGetAbsoluePath(const std::string& title = "Open");

std::filesystem::path openFolderExplorerAndGetRelativePath(const std::string& title = "Open");

void recycleFileOrDirectory(const std::filesystem::path& path);

void SystemOpen(const std::string& path);
