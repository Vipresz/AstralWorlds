#pragma once

#include "render/Animation.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Game {
namespace Character {

class SkillClipCache {
public:
    static SkillClipCache& Instance();

    std::shared_ptr<Engine::Platform::Render::Animation> GetOrLoad(const std::string& path);
    void Clear();

private:
    SkillClipCache() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<Engine::Platform::Render::Animation>> m_entries;
};

} // namespace Character
} // namespace Game
