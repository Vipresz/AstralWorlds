#include "character/SkillClipCache.h"

#include "Logger.h"
#include "render/Animation.h"

#include <fstream>

using Engine::Platform::Render::Animation;
using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

bool FileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

} // namespace

SkillClipCache& SkillClipCache::Instance() {
    static SkillClipCache cache;
    return cache;
}

std::shared_ptr<Animation> SkillClipCache::GetOrLoad(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto existing = m_entries.find(path);
    if (existing != m_entries.end()) {
        return existing->second;
    }

    if (!FileExists(path)) {
        LOG_WARNING_FMT("SkillClipCache", "Skill animation missing: %s", path.c_str());
        return nullptr;
    }

    auto animation = std::make_shared<Animation>();
    if (!animation->LoadAnimation(path, true)) {
        LOG_ERROR_FMT("SkillClipCache", "Failed to load skill animation: %s", path.c_str());
        return nullptr;
    }

    m_entries[path] = animation;
    LOG_INFO_FMT("SkillClipCache", "Cached skill animation: %s", path.c_str());
    return animation;
}

void SkillClipCache::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

} // namespace Character
} // namespace Game
