#pragma once

#include "Engine/UpdateMetadata.hpp"
#include "Engine/Vector2.hpp"
#include "Game/GameData.hpp"
#include "Game/UIMenu.hpp"

#include <functional>
#include <string>
#include <vector>

class UpdateMenu : public UIMenu
{
protected:
    UpdateMetadata m_metadata;
    std::string    changelog;
    std::vector<std::string> lines;
    std::string windowName;
    std::string statusText;

    std::function<bool(GameData&, const UpdateMetadata&)> m_onInstall;

    void appendStatus(const std::string& message);

public:
    UpdateMenu(GameData& inDatas, Vec2 inPosition, const UpdateMetadata& metadata,
               std::function<bool(GameData&, const UpdateMetadata&)> onInstall);

    virtual ~UpdateMenu() = default;

    void update(double deltaTime);
};
