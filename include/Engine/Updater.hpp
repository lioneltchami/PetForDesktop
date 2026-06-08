#pragma once

#include "Engine/Log.hpp"
#include "Engine/Singleton.hpp"
#include "Engine/UpdateMetadata.hpp"
#include "Game/GameData.hpp"

#include <filesystem>
#include <string>

class Updater : public Singleton<Updater>
{
public:
    bool checkForUpdate(GameData& datas);

    bool isVersionAvailable(const std::string& currentTag, const std::string& remoteTag) const;

    bool fetchReleaseMetadata(UpdateMetadata& metadata, std::string& error);

    bool resolvePlatformPackage(const UpdateMetadata& metadata, UpdateMetadata& resolved, std::string& error) const;

    bool validateMetadataEnvelope(const UpdateMetadata& metadata, std::string& error) const;

    bool verifySignedMetadata(const UpdateMetadata& metadata, std::string& error) const;

    bool downloadAndStageUpdate(const UpdateMetadata& metadata, std::filesystem::path& stagedFile, std::string& error) const;

    bool verifyDownloadedPackage(const std::filesystem::path& stagedFile, const UpdateMetadata& metadata,
                                std::string& error) const;

    bool applyPackage(const std::filesystem::path& stagedFile, const UpdateMetadata& metadata, std::string& error) const;

    static bool parseManifestForTest(const std::string& manifestText, UpdateMetadata& metadata);

    static bool validateMetadataEnvelopeForTest(const UpdateMetadata& metadata, std::string& error);

    static bool verifySignedMetadataForTest(const UpdateMetadata& metadata, std::string& error);
};
