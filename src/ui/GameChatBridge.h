#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "ui/ChatWindow.h"

#include <memory>
#include <string>

namespace Engine {
namespace ECS {
class Registry;
}
namespace Net {
struct ChatMessage;
}
}

namespace Engine {
namespace Platform {
namespace Networking {
namespace Systems {
class NetworkSystem;
}
} // namespace Networking
} // namespace Platform
} // namespace Engine

namespace Game {
namespace UI {

// Connects ChatWindow UI to the multiplayer network stack.
class GameChatBridge {
public:
    void Attach(ChatWindow& chatWindow,
                const std::shared_ptr<Engine::Platform::Networking::Systems::NetworkSystem>& networkSystem,
                Engine::ECS::Registry& registry,
                const std::string& localDisplayName);
    void SetOnline(bool online);

private:
    void SendMessage(const std::string& text) const;
    void AppendLocalMessage(const std::string& displayName, const std::string& text) const;
    bool ShouldIgnoreIncoming(const Engine::Net::ChatMessage& message) const;

    void ShowSpeechBubbleForUser(Engine::Net::UserId userId, const std::string& text) const;

    ChatWindow* m_chatWindow = nullptr;
    std::shared_ptr<Engine::Platform::Networking::Systems::NetworkSystem> m_networkSystem;
    Engine::ECS::Registry* m_registry = nullptr;
    std::string m_localDisplayName;
    mutable std::string m_lastOptimisticText;
    bool m_online = false;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
