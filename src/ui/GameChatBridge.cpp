#include "ui/GameChatBridge.h"

#ifdef PLATFORM_HAS_NUKLEAR

#include "character/SpeechBubbleComponent.h"
#include "core/ecs/Registry.h"
#include "core/network/ChatMessage.h"
#include "core/network/MovementCommand.h"
#include "network/systems/NetworkSystem.h"

namespace Game {
namespace UI {

void GameChatBridge::Attach(
    ChatWindow& chatWindow,
    const std::shared_ptr<Engine::Platform::Networking::Systems::NetworkSystem>& networkSystem,
    Engine::ECS::Registry& registry,
    const std::string& localDisplayName) {
    m_chatWindow = &chatWindow;
    m_networkSystem = networkSystem;
    m_registry = &registry;
    m_localDisplayName = localDisplayName;

    chatWindow.SetEnabled(false);
    chatWindow.SetSendHandler([this](const std::string& text) { SendMessage(text); });

    networkSystem->SetGenericDataHandler([this](const Engine::Net::GenericDataPayload& payload) {
        if (!m_chatWindow || payload.channel != Engine::Net::kChatChannel) {
            return;
        }
        Engine::Net::ChatMessage message;
        if (!Engine::Net::DeserializeChatMessage(Engine::Net::NetBuffer(payload.data), message)) {
            return;
        }
        if (ShouldIgnoreIncoming(message)) {
            return;
        }
        ShowSpeechBubbleForUser(message.userId, message.text);
        ChatLine line;
        line.userId = message.userId;
        line.displayName = std::move(message.displayName);
        line.text = std::move(message.text);
        m_chatWindow->AppendMessage(std::move(line));
    });
}

void GameChatBridge::SetOnline(bool online) {
    m_online = online;
    if (!m_chatWindow) {
        return;
    }

    m_chatWindow->SetEnabled(online);
    if (online) {
        AppendLocalMessage("System", "Connected to world chat. Press Ctrl+L or Enter over chat to type.");
    }
}

void GameChatBridge::AppendLocalMessage(const std::string& displayName, const std::string& text) const {
    if (!m_chatWindow) {
        return;
    }
    ChatLine line;
    line.displayName = displayName;
    line.text = text;
    m_chatWindow->AppendMessage(std::move(line));
}

bool GameChatBridge::ShouldIgnoreIncoming(const Engine::Net::ChatMessage& message) const {
    if (!m_networkSystem || message.text != m_lastOptimisticText) {
        return false;
    }
    return message.userId == m_networkSystem->GetLocalUserId();
}

void GameChatBridge::ShowSpeechBubbleForUser(Engine::Net::UserId userId,
                                             const std::string& text) const {
    if (!m_registry || !m_networkSystem || text.empty()) {
        return;
    }

    const Engine::EntityID entity = m_networkSystem->TryGetPlayerEntity(userId);
    if (entity == Engine::INVALID_ENTITY) {
        return;
    }
    Game::Character::ShowSpeechBubble(*m_registry, entity, text.c_str());
}

void GameChatBridge::SendMessage(const std::string& text) const {
    if (text.empty() || !m_chatWindow) {
        return;
    }

    m_lastOptimisticText = text;
    AppendLocalMessage(m_localDisplayName.empty() ? "You" : m_localDisplayName, text);
    if (m_networkSystem) {
        ShowSpeechBubbleForUser(m_networkSystem->GetLocalUserId(), text);
    }

    if (!m_online || !m_networkSystem || !m_networkSystem->IsLoggedIn()) {
        return;
    }

    Engine::Net::ChatCommand command;
    command.text = text;
    Engine::Net::GenericDataPayload payload;
    payload.userId = m_networkSystem->GetLocalUserId();
    payload.tick = 0;
    payload.channel = Engine::Net::kChatChannel;
    payload.data = Engine::Net::SerializeChatCommand(command).Data();
    m_networkSystem->GetClient().SendGenericData(payload);
}

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
