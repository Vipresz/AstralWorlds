#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "Menu.h"
#include "core/network/NetTypes.h"
#include "render/Texture.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <string>

struct nk_context;
struct nk_rect;

namespace Engine {
namespace Platform {
class Window;
namespace UI {
class FontLibrary;
}
}
}

namespace Game {
namespace UI {

struct ChatLine {
    Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
    std::string displayName;
    std::string text;
};

// Bottom-left shared chat log. Press Ctrl+L or Enter (over panel) to type; Enter again to send.
class ChatWindow {
public:
    using SendHandler = std::function<void(const std::string& text)>;

    bool Initialize(const ChatHudConfig& config, const MenuReferenceScreen& referenceScreen);
    void Shutdown();

    void SetSendHandler(SendHandler handler) { m_sendHandler = std::move(handler); }
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    bool IsInputFocused() const { return m_inputFocused; }
    bool BlocksGameInput() const {
        return m_enabled &&
               (m_inputFocused || (m_config.movable && m_dragging) || m_pointerOver);
    }

    void AppendMessage(ChatLine line);
    void CancelInput();
    void Update(Engine::Platform::Window& window);
    void Draw(struct nk_context* ctx,
              const Engine::Platform::UI::FontLibrary& fonts,
              Engine::Platform::Window& window,
              int screenWidth,
              int screenHeight);

private:
    struct ChatBounds {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float padding = 0.0f;
        float inputRowHeight = 0.0f;
        float logRowHeight = 0.0f;
    };

    void MaybeReloadLayoutFromManifest();
    ChatBounds ResolveBounds(int screenWidth, int screenHeight);
    void InitializePanelPosition(int screenWidth, int screenHeight, float panelWidth, float panelHeight);
    void ComputeAnchoredPosition(int screenWidth,
                                 int screenHeight,
                                 float panelWidth,
                                 float panelHeight,
                                 float& outX,
                                 float& outY) const;
    void UpdatePointer(struct nk_context* ctx,
                       Engine::Platform::Window& window,
                       int screenWidth,
                       int screenHeight,
                       const ChatBounds& bounds);
    bool IsMouseOverPanel(const Engine::Platform::Window& window) const;
    void HandleScroll(Engine::Platform::Window& window);
    static struct nk_rect ResolveContentBounds(const ChatBounds& panel, const ChatContentLayoutConfig& content);
    static void ClampPanelPosition(
        float& x, float& y, float width, float height, int screenWidth, int screenHeight);

    void SyncCharInputHandler(Engine::Platform::Window& window);
    bool KeyPressed(Engine::Platform::Window& window, int key, bool& wasDown) const;
    void TrySendInput();
    void AppendTypedChar(char ch);

    ChatHudConfig m_config;
    MenuReferenceScreen m_referenceScreen;
    std::unique_ptr<Engine::Platform::Render::Texture> m_texture;
    SendHandler m_sendHandler;
    std::deque<ChatLine> m_lines;
    std::string m_input;
    std::uint64_t m_lastManifestWriteTime = 0;
    bool m_hasManifestWriteTime = false;
    bool m_enabled = true;
    bool m_inputFocused = false;
    bool m_enterWasDown = false;
    bool m_kpEnterWasDown = false;
    bool m_escapeWasDown = false;
    bool m_backspaceWasDown = false;
    bool m_ctrlLWasDown = false;
    bool m_charHandlerInstalled = false;
    bool m_positionInitialized = false;
    bool m_dragging = false;
    bool m_hovered = false;
    bool m_pointerOver = false;
    bool m_pointerDown = false;
    float m_pointerDownX = 0.0f;
    float m_pointerDownY = 0.0f;
    float m_panelX = 0.0f;
    float m_panelY = 0.0f;
    float m_dragGrabX = 0.0f;
    float m_dragGrabY = 0.0f;
    float m_lastPanelWidth = 0.0f;
    float m_lastPanelHeight = 0.0f;
    int m_logScrollOffset = 0;
    static constexpr float kDragThresholdPixels = 5.0f;
    static constexpr float kScrollbarWidth = 4.0f;
    static constexpr size_t kMaxStoredLines = 100;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
