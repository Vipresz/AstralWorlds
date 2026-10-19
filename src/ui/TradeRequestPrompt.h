#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "core/ecs/components/PlayerTradeComponent.h"
#include "core/network/NetTypes.h"
#include "core/network/TradeSession.h"
#include "ui/DecisionPrompt.h"

#include <functional>
#include <string>

struct nk_context;

namespace Game {
namespace UI {

// Top-center prompt for incoming/outgoing trade requests.
class TradeRequestPrompt {
public:
    using TradeCommandCallback = std::function<void(const Engine::Net::TradeCommand&)>;

    void SetTradeCommandCallback(TradeCommandCallback callback);

    void Sync(const Engine::ECS::PlayerTradeComponent* trade,
              Engine::Net::UserId localUserId,
              const std::string& partnerDisplayName);

    bool IsVisible() const { return m_prompt.IsVisible(); }
    bool BlocksGameInput() const { return m_prompt.BlocksGameInput(); }

    void Draw(struct nk_context* ctx, int screenWidth, int screenHeight);

private:
    Engine::Platform::UI::DecisionPrompt m_prompt;
    TradeCommandCallback m_tradeCommandCallback;
    Engine::Net::UserId m_partnerUserId = Engine::Net::kInvalidUserId;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
