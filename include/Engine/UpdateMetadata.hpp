#pragma once

#include <string>
#include <vector>

struct UpdateAssetMetadata
{
    std::string name;
    std::string downloadUrl;
    std::string size;
};

struct UpdateMetadata
{
    std::string         tag;
    std::string         packageUrl;
    std::string         packageName;
    std::string         checksum;
    std::string         checksumAlgorithm = "sha256";
    std::string         signature;
    std::string         releaseNotes;
    std::vector<UpdateAssetMetadata> assets;
};
