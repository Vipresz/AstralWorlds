#include "ui/TradeRequestPrompt.h"

#ifdef PLATFORM_HAS_NUKLEAR

namespace Game {
namespace UI {

void TradeRequestPrompt::SetTradeCommandCallback(TradeCommandCallback callback) {
    m_tradeCommandCallback = std::move(callback);
}

void TradeRequestPrompt::Sync(const Engine::ECS::PlayerTradeComponent* trade,
                              Engine::Net::UserId localUserId,
                              const std::string& partnerDisplayName) {
    if (!trade || trade->phase != Engine::ECS::TradePhase::Requested ||
        localUserId == Engine::Net::kInvalidUserId) {
        m_prompt.SetVisible(false);
        m_partnerUserId = Engine::Net::kInvalidUserId;
        return;
    }

    m_partnerUserId = trade->partnerUserId;
    const std::string partnerName =
        partnerDisplayName.empty() ? "another player" : partnerDisplayName;
    const bool isIncoming =
        trade->requesterUserId != Engine::Net::kInvalidUserId &&
        localUserId != trade->requesterUserId;

    Engine::Platform::UI::DecisionPromptContent content;
    content.title = "Trade Request";
    if (isIncoming) {
        content.message = partnerName + " wants to trade with you.";
        content.primaryLabel = "Accept";
        content.secondaryLabel = "Decline";
        content.showPrimary = true;
        content.showSecondary = true;
    } else {
        content.message = "Waiting for " + partnerName + " to accept your trade request.";
        content.primaryLabel = "";
        content.secondaryLabel = "Cancel";
        content.showPrimary = false;
        content.showSecondary = true;
    }

    m_prompt.SetContent(std::move(content));
    m_prompt.SetVisible(true);

    m_prompt.SetPrimaryCallback([this, requesterUserId = trade->requesterUserId]() {
        if (!m_tradeCommandCallback) {
            return;
        }
        Engine::Net::TradeCommand command;
        command.action = Engine::Net::TradeAction::Accept;
        command.targetUserId = requesterUserId;
        m_tradeCommandCallback(command);
        m_prompt.SetVisible(false);
    });

    m_prompt.SetSecondaryCallback([this]() {
        if (!m_tradeCommandCallback) {
            m_prompt.SetVisible(false);
            return;
        }
        Engine::Net::TradeCommand command;
        command.action = Engine::Net::TradeAction::Cancel;
        command.targetUserId = m_partnerUserId;
        m_tradeCommandCallback(command);
        m_prompt.SetVisible(false);
    });
}

void TradeRequestPrompt::Draw(struct nk_context* ctx, int screenWidth, int screenHeight) {
    m_prompt.Draw(ctx, screenWidth, screenHeight);
}

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
