#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct UpdateAssetMetadata
{
    std::string name;
    std::string downloadUrl;
    std::string size;
    std::uint64_t sizeBytes = 0;
};

struct UpdateMetadata
{
    std::string         tag;
    std::string         packageUrl;
    std::string         packageName;
    std::string         checksum;
    std::string         checksumAlgorithm = "sha256";
    std::string         signature;
    std::string         signatureAlgorithm;
    std::string         signaturePublicKey;
    std::string         releaseNotes;
    std::uint64_t       packageSize = 0;
    std::vector<UpdateAssetMetadata> assets;
};
