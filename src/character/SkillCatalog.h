#pragma once

#include "character/SkillDefinition.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Game {
namespace Character {

class SkillCatalog {
public:
    bool Load(const char* catalogPath = "character/skills/catalog.json");

    const SkillDefinition* Find(const std::string& id) const;
    const std::vector<SkillDefinition>& GetAll() const { return m_skills; }
    const SkillDefinition* FindByHotkey(const std::string& hotkey) const;

private:
    bool LoadManifest(const std::string& manifestPath, SkillDefinition& out);

    std::vector<SkillDefinition> m_skills;
    std::unordered_map<std::string, size_t> m_byId;
};

} // namespace Character
} // namespace Game
