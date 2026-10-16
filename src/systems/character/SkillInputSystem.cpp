#include "systems/character/SkillInputSystem.h"

#include "character/SkillActivator.h"

#include <GLFW/glfw3.h>
#include <string>

namespace Game {
namespace Character {

namespace {

constexpr int HotkeyIndexFromGlfwKey(int key) {
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        return key - GLFW_KEY_1;
    }
    return -1;
}

} // namespace

SkillInputSystem::SkillInputSystem(Engine::Platform::Window* window, const SkillCatalog* catalog)
    : m_window(window), m_catalog(catalog) {
    SetPriority(1);
}

void SkillInputSystem::Initialize(Engine::ECS::Registry& registry) {
    (void)registry;
}

void SkillInputSystem::Shutdown(Engine::ECS::Registry& registry) {
    (void)registry;
}

void SkillInputSystem::Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t tickNumber) {
    (void)deltaTime;
    (void)tickNumber;

    if (!m_window || !m_catalog || m_characterEntityId == 0) {
        return;
    }
    if (m_inputBlockQuery && m_inputBlockQuery()) {
        return;
    }

    bool* keys = m_window->getsKeys();
    for (int key = GLFW_KEY_1; key <= GLFW_KEY_9; ++key) {
        const int index = HotkeyIndexFromGlfwKey(key);
        if (index < 0 || index >= 10) {
            continue;
        }

        const bool pressed = keys[key];
        const bool justPressed = pressed && !m_hotkeyPressedLastFrame[index];
        m_hotkeyPressedLastFrame[index] = pressed;
        if (!justPressed) {
            continue;
        }

        const SkillDefinition* skill = nullptr;
        if (m_slotSkillResolver) {
            skill = m_slotSkillResolver(index);
        }
        if (!skill && m_catalog) {
            const std::string hotkey = std::to_string(index + 1);
            skill = m_catalog->FindByHotkey(hotkey);
        }
        if (!skill || !m_catalog) {
            continue;
        }

        SkillActivator::TryActivate(registry, m_characterEntityId, *m_catalog, *skill);
    }
}

} // namespace Character
} // namespace Game
