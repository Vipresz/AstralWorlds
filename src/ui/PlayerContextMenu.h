#pragma once

#include "ui/PlayerTargeting.h"

#include <functional>
#include <glm/glm.hpp>
#include <memory>

struct nk_context;
struct nk_rect;

namespace Engine {
namespace Platform {
class Window;
namespace Render {
class ICameraSource;
}
namespace Networking {
namespace Systems {
class NetworkSystem;
}
} // namespace Networking
} // namespace Platform
namespace ECS {
class Registry;
}
} // namespace Engine

namespace Game {
namespace UI {

enum class PlayerContextAction {
    Inspect,
    Follow,
    Trade,
};

// Optional screen region (e.g. target player HUD) that opens the same context menu on RMB.
struct PlayerHudContextHit {
    bool active = false;
    PlayerPickResult target{};
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

class PlayerContextMenu {
public:
    using ActionCallback = std::function<void(PlayerContextAction action, const PlayerPickResult& target)>;

    void SetActionCallback(ActionCallback callback) { m_actionCallback = std::move(callback); }

    bool IsOpen() const { return m_open; }
    bool BlocksGameInput() const { return m_open; }

    void Close();

    void Update(Engine::Platform::Window& window,
                Engine::ECS::Registry& registry,
                Engine::Platform::Networking::Systems::NetworkSystem* network,
                const Engine::Platform::Render::ICameraSource* camera,
                const glm::mat4& projection,
                int screenWidth,
                int screenHeight,
                bool gameInputBlocked,
                const PlayerHudContextHit& hudContextHit = {});

    void Build(struct nk_context* ctx, int screenWidth, int screenHeight);

private:
    struct nk_rect ComputeMenuBounds(int screenWidth, int screenHeight) const;
    struct nk_rect ButtonBounds(int buttonIndex, const struct nk_rect& menuBounds) const;

    bool m_open = false;
    float m_anchorX = 0.0f;
    float m_anchorY = 0.0f;
    float m_menuX = 0.0f;
    float m_menuY = 0.0f;
    float m_menuW = 0.0f;
    float m_menuH = 0.0f;
    PlayerPickResult m_target{};
    bool m_rmbWasDown = false;
    bool m_lmbWasDown = false;
    float m_rmbDownX = 0.0f;
    float m_rmbDownY = 0.0f;
    ActionCallback m_actionCallback;
};

std::unique_ptr<PlayerContextMenu> CreatePlayerContextMenu();

} // namespace UI
} // namespace Game
