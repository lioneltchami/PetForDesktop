#pragma once

#include "Engine/Singleton.hpp"
#include "Game/GameData.hpp"

#include <string>
#include <vector>

class Setting : public Singleton<Setting>
{
public:
    struct ValidationIssue
    {
        std::string filePath;
        std::string section;
        std::string field;
        std::string message;
        std::string severity; // "warning" or "error"
    };

    struct ValidationReport
    {
        bool                          valid = true;
        std::vector<ValidationIssue>   warnings;
        std::vector<ValidationIssue>   errors;
    };

    bool importFile(const char* src, GameData& data, ValidationReport& report);

    void importFile(const char* src, GameData& data);

    void exportFile(const char* dest, GameData& data);

    static bool validateForRuntime(const GameData& data, ValidationReport& report);

    static bool sanitize(GameData& data);

    static void clampForRuntime(GameData& data);
};
