#include "Game/UpdateMenu.hpp"

#include "Engine/StylePanel.hpp"
#include "Engine/Log.hpp"
#include "Engine/Updater.hpp"

#include <algorithm>
#include <sstream>

UpdateMenu::UpdateMenu(GameData& inDatas, Vec2 inPosition, const UpdateMetadata& metadata,
                       std::function<bool(GameData&, const UpdateMetadata&)> onInstall)
    : UIMenu(inDatas, inPosition, Vec2(400.f, 200.f)), m_metadata{metadata}, m_onInstall{std::move(onInstall)}
{
    windowName = std::string(PROJECT_NAME " ") + metadata.tag + " is available.";
    changelog  = metadata.releaseNotes;
    m_metadata = metadata;

    m_isMetadataValid = Updater::instance().validateMetadataEnvelope(m_metadata, m_metadataError);
    if (m_isMetadataValid && !metadata.signature.empty())
    {
        m_isMetadataValid = Updater::instance().verifySignedMetadata(m_metadata, m_metadataError);
    }

    // Normalize markdown formatting to plain text for quick display in ImGui.
    std::string current;
    for (char ch : changelog)
    {
        if (ch == '\n')
            current += '\n';
        else if (ch == '\r')
            continue;
        else
            current += ch;
    }
    changelog = current;
}

void UpdateMenu::appendStatus(const std::string& message)
{
    statusText = message;
}

void UpdateMenu::update(double deltaTime)
{
    (void)deltaTime;
    windowBegin();

    bool isWindowOpen = true;

    ImGui::Begin(windowName.c_str(), &isWindowOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    shouldClose = !isWindowOpen;

    ImGui::Text("%s", m_metadata.tag.c_str());
    if (!m_metadata.packageName.empty())
        ImGui::Text("Package: %s", m_metadata.packageName.c_str());

    if (!m_metadata.checksum.empty())
    {
        ImGui::Text("Checksum: %s (%s)", m_metadata.checksumAlgorithm.c_str(), m_metadata.checksum.c_str());
    }
    else
    {
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "No checksum metadata available");
    }

    if (!m_metadata.signature.empty())
        ImGui::Text("Signed metadata detected");

    if (!m_isMetadataValid)
    {
        ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Invalid update metadata: %s", m_metadataError.c_str());
    }

    const bool canInstall = m_isMetadataValid && !m_metadata.packageUrl.empty() &&
                           (m_metadata.packageSize == 0 || m_metadata.packageSize <= 300u * 1024u * 1024u) &&
                           !m_metadata.packageName.empty();
    if (!canInstall)
    {
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "Update blocked: package metadata is incomplete or untrusted.");
    }

    if (!changelog.empty())
    {
        ImGui::Separator();
        std::istringstream       iss(changelog);
        std::vector<std::string> lines;
        std::string              line;
        while (std::getline(iss, line))
            lines.push_back(line);

        for (const auto& item : lines)
            ImGui::TextWrapped("%s", item.c_str());
    }

    if (!statusText.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped("%s", statusText.c_str());
    }

    ImGui::Separator();

    if (!canInstall)
        ImGui::BeginDisabled();

    if (ImGui::Button("Update", ImVec2(ImGui::GetContentRegionAvail().x, 0.f)))
    {
        if (!m_metadata.packageUrl.empty() && m_onInstall)
        {
            appendStatus("Preparing update download...");
            if (m_onInstall(datas, m_metadata))
            {
                shouldClose = true;
            }
            else
            {
                appendStatus("Update failed. Check debug output for details.");
            }
        }
        else
        {
            appendStatus("No valid package URL to install.");
        }
    }

    if (!canInstall)
        ImGui::EndDisabled();

    windowEnd();
    ImGui::End();
}
