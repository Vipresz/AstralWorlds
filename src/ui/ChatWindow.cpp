#include "ui/ChatWindow.h"
#include "ui/ChatInputFocus.h"

#ifdef PLATFORM_HAS_NUKLEAR

#include "Logger.h"
#include "MenuLayout.h"
#include "core/network/ChatMessage.h"
#include "render/Window.h"
#include "ui/FontLibrary.h"
#include "ui/OverlayWindow.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include <GLFW/glfw3.h>

using Engine::Platform::Render::Texture;
using Engine::Platform::UI::DrawTexture;
using Engine::Platform::UI::OverlayScope;
using EngineLogging::Logger;

namespace Game {
namespace UI {

namespace {

std::uint64_t FileWriteTimeToken(const char* path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto writeTime = fs::last_write_time(path, ec);
    if (ec) {
        return 0;
    }
    return static_cast<std::uint64_t>(writeTime.time_since_epoch().count());
}

constexpr const char* kRuntimeManifestPath = "menus/manifest.json";
constexpr const char* kDevSrcManifestPath = "../../../src/menus/manifest.json";

struct ManifestWatch {
    const char* path = kRuntimeManifestPath;
    std::uint64_t writeTime = 0;
};

ManifestWatch ResolveChatManifest() {
    const std::uint64_t srcWriteTime = FileWriteTimeToken(kDevSrcManifestPath);
    if (srcWriteTime != 0) {
        return ManifestWatch{kDevSrcManifestPath, srcWriteTime};
    }
    return ManifestWatch{kRuntimeManifestPath, FileWriteTimeToken(kRuntimeManifestPath)};
}

bool ApplyManifestLayout(ChatHudConfig& config, const char* manifestPath) {
    CharacterMenuConfig loaded;
    if (!CharacterMenu::LoadConfig(loaded, manifestPath)) {
        return false;
    }
    config = loaded.chat;
    return true;
}
} // namespace

bool ChatWindow::Initialize(const ChatHudConfig& config, const MenuReferenceScreen& referenceScreen) {
    Shutdown();
    m_config = config;
    m_referenceScreen = referenceScreen;

    const ManifestWatch watch = ResolveChatManifest();
    if (watch.writeTime != 0) {
        ApplyManifestLayout(m_config, watch.path);
    }
    m_lastManifestWriteTime = watch.writeTime;
    m_hasManifestWriteTime = watch.writeTime != 0;

    if (m_config.texturePath.empty()) {
        LOG_WARNING("ChatWindow", "No chat background texture configured");
        return false;
    }

    m_texture = std::make_unique<Texture>(m_config.texturePath.c_str());
    if (!m_texture->LoadTextureUI(1024)) {
        LOG_ERROR_FMT("ChatWindow", "Failed to load chat background: %s", m_config.texturePath.c_str());
        m_texture.reset();
        return false;
    }

    LOG_INFO_FMT("ChatWindow",
                 "Loaded %s from %s size=%dx%d anchor=%s",
                 m_config.texturePath.c_str(),
                 watch.path,
                 m_config.width,
                 m_config.height,
                 m_config.anchor.c_str());
    return true;
}

void ChatWindow::Shutdown() {
    m_texture.reset();
    m_lines.clear();
    m_input.clear();
    m_lastManifestWriteTime = 0;
    m_hasManifestWriteTime = false;
    m_inputFocused = false;
    m_charHandlerInstalled = false;
    m_positionInitialized = false;
    m_dragging = false;
    m_hovered = false;
    m_pointerOver = false;
    m_pointerDown = false;
    m_logScrollOffset = 0;
}

void ChatWindow::MaybeReloadLayoutFromManifest() {
    const ManifestWatch watch = ResolveChatManifest();
    if (watch.writeTime == 0) {
        return;
    }
    if (m_hasManifestWriteTime && watch.writeTime == m_lastManifestWriteTime) {
        return;
    }
    m_lastManifestWriteTime = watch.writeTime;
    m_hasManifestWriteTime = true;

    const std::string previousTexture = m_config.texturePath;
    if (!ApplyManifestLayout(m_config, watch.path)) {
        return;
    }

    if (m_config.texturePath != previousTexture || !m_texture || !m_texture->IsLoaded()) {
        m_texture = std::make_unique<Texture>(m_config.texturePath.c_str());
        if (!m_texture->LoadTextureUI(1024)) {
            LOG_ERROR_FMT("ChatWindow",
                          "Failed to reload chat background: %s",
                          m_config.texturePath.c_str());
            m_texture.reset();
        }
    }

    LOG_INFO_FMT("ChatWindow",
                 "Reloaded layout from %s (size=%dx%d)",
                 watch.path,
                 m_config.width,
                 m_config.height);
}

ChatWindow::ChatBounds ChatWindow::ResolveBounds(int screenWidth, int screenHeight) {
    ChatBounds bounds;
    const MenuScreenLayout layout =
        MenuScreenLayout::From(screenWidth, screenHeight, m_referenceScreen);

    bounds.width = layout.Scale(static_cast<float>(std::max(1, m_config.width)));
    bounds.height = layout.Scale(static_cast<float>(std::max(1, m_config.height)));
    bounds.padding = layout.Scale(m_config.paddingPixels);
    bounds.inputRowHeight = layout.Scale(m_config.inputRowHeight);
    bounds.logRowHeight = layout.Scale(std::max(8.0f, m_config.logRowHeight));

    if (!m_positionInitialized) {
        InitializePanelPosition(screenWidth, screenHeight, bounds.width, bounds.height);
    }

    if (m_config.movable) {
        bounds.x = m_panelX;
        bounds.y = m_panelY;
    } else {
        float x = 0.0f;
        float y = 0.0f;
        ComputeAnchoredPosition(screenWidth, screenHeight, bounds.width, bounds.height, x, y);
        bounds.x = x;
        bounds.y = y;
    }
    return bounds;
}

void ChatWindow::InitializePanelPosition(
    int screenWidth, int screenHeight, float panelWidth, float panelHeight) {
    float x = 0.0f;
    float y = 0.0f;
    ComputeAnchoredPosition(screenWidth, screenHeight, panelWidth, panelHeight, x, y);
    m_panelX = x;
    m_panelY = y;
    m_positionInitialized = true;
}

void ChatWindow::ComputeAnchoredPosition(int screenWidth,
                                         int screenHeight,
                                         float panelWidth,
                                         float panelHeight,
                                         float& outX,
                                         float& outY) const {
    outX = static_cast<float>(screenWidth) * m_config.xFraction;
    outY = static_cast<float>(screenHeight) * m_config.yFraction;
    if (m_config.anchor == "top_right") {
        outX = static_cast<float>(screenWidth) * (1.0f - m_config.xFraction) - panelWidth;
    } else if (m_config.anchor == "bottom_left") {
        outY = static_cast<float>(screenHeight) * (1.0f - m_config.yFraction) - panelHeight;
    } else if (m_config.anchor == "bottom_right") {
        outX = static_cast<float>(screenWidth) * (1.0f - m_config.xFraction) - panelWidth;
        outY = static_cast<float>(screenHeight) * (1.0f - m_config.yFraction) - panelHeight;
    }
    ClampPanelPosition(outX, outY, panelWidth, panelHeight, screenWidth, screenHeight);
}

void ChatWindow::ClampPanelPosition(
    float& x, float& y, float width, float height, int screenWidth, int screenHeight) {
    const float maxX = std::max(0.0f, static_cast<float>(screenWidth) - width);
    const float maxY = std::max(0.0f, static_cast<float>(screenHeight) - height);
    x = std::clamp(x, 0.0f, maxX);
    y = std::clamp(y, 0.0f, maxY);
}

bool ChatWindow::IsMouseOverPanel(const Engine::Platform::Window& window) const {
    if (m_lastPanelWidth <= 0.0f || m_lastPanelHeight <= 0.0f) {
        return false;
    }
    GLFWwindow* glfwWindow = window.GetGLFWWindow();
    if (!glfwWindow) {
        return false;
    }
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(glfwWindow, &mouseX, &mouseY);
    return mouseX >= m_panelX && mouseX <= m_panelX + m_lastPanelWidth && mouseY >= m_panelY &&
           mouseY <= m_panelY + m_lastPanelHeight;
}

void ChatWindow::HandleScroll(Engine::Platform::Window& window) {
    if (!m_enabled || !IsMouseOverPanel(window)) {
        return;
    }

    const float scrollDelta = window.getScrollYChange();
    if (scrollDelta == 0.0f) {
        return;
    }

    const int step = std::max(1, static_cast<int>(std::round(
        std::abs(scrollDelta) * std::max(0.25f, m_config.scrollLinesPerWheel))));
    if (scrollDelta > 0.0f) {
        m_logScrollOffset += step;
    } else {
        m_logScrollOffset = std::max(0, m_logScrollOffset - step);
    }
}

struct nk_rect ChatWindow::ResolveContentBounds(const ChatBounds& panel,
                                              const ChatContentLayoutConfig& content) {
    const float xFraction = std::clamp(content.xFraction, 0.0f, 0.95f);
    const float yFraction = std::clamp(content.yFraction, 0.0f, 0.95f);
    const float widthFraction = std::clamp(content.widthFraction, 0.05f, 1.0f - xFraction);
    const float heightFraction = std::clamp(content.heightFraction, 0.05f, 1.0f - yFraction);
    return nk_rect(panel.x + panel.width * xFraction,
                   panel.y + panel.height * yFraction,
                   panel.width * widthFraction,
                   panel.height * heightFraction);
}

void ChatWindow::UpdatePointer(
    struct nk_context* ctx,
    Engine::Platform::Window& window,
    int screenWidth,
    int screenHeight,
    const ChatBounds& bounds) {
    m_hovered = false;
    m_pointerOver = false;
    if (!ctx || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    if (!m_config.movable) {
        m_dragging = false;
        m_pointerDown = false;
        const struct nk_rect panelBounds = nk_rect(bounds.x, bounds.y, bounds.width, bounds.height);
        m_hovered = nk_input_is_mouse_hovering_rect(&ctx->input, panelBounds);
        m_pointerOver = m_hovered || m_inputFocused;
        m_lastPanelWidth = bounds.width;
        m_lastPanelHeight = bounds.height;
        return;
    }

    const struct nk_rect panelBounds = nk_rect(bounds.x, bounds.y, bounds.width, bounds.height);
    m_hovered = nk_input_is_mouse_hovering_rect(&ctx->input, panelBounds);
    m_pointerOver = m_hovered || m_dragging || m_inputFocused;
    m_lastPanelWidth = bounds.width;
    m_lastPanelHeight = bounds.height;

    const bool leftDown = ctx->input.mouse.buttons[NK_BUTTON_LEFT].down;
    const bool leftPressed =
        nk_input_is_mouse_click_down_in_rect(&ctx->input, NK_BUTTON_LEFT, panelBounds, nk_true);

    if (m_dragging) {
        if (leftDown) {
            m_panelX = ctx->input.mouse.pos.x - m_dragGrabX;
            m_panelY = ctx->input.mouse.pos.y - m_dragGrabY;
            ClampPanelPosition(
                m_panelX, m_panelY, bounds.width, bounds.height, screenWidth, screenHeight);
        } else {
            m_dragging = false;
        }
        m_pointerDown = false;
        return;
    }

    if (leftPressed) {
        m_pointerDown = true;
        m_pointerDownX = ctx->input.mouse.pos.x;
        m_pointerDownY = ctx->input.mouse.pos.y;
        m_dragGrabX = ctx->input.mouse.pos.x - bounds.x;
        m_dragGrabY = ctx->input.mouse.pos.y - bounds.y;
        return;
    }

    if (m_pointerDown && leftDown) {
        const float dx = ctx->input.mouse.pos.x - m_pointerDownX;
        const float dy = ctx->input.mouse.pos.y - m_pointerDownY;
        if ((dx * dx + dy * dy) > (kDragThresholdPixels * kDragThresholdPixels)) {
            m_dragging = true;
            m_pointerDown = false;
            m_inputFocused = false;
            SyncCharInputHandler(window);
        }
        return;
    }

    if (m_pointerDown && !leftDown) {
        if (m_hovered) {
            m_inputFocused = true;
            SyncCharInputHandler(window);
        }
        m_pointerDown = false;
    }
}

void ChatWindow::AppendMessage(ChatLine line) {
    if (line.text.empty()) {
        return;
    }
    const bool pinnedToBottom = m_logScrollOffset == 0;
    m_lines.push_back(std::move(line));
    while (m_lines.size() > kMaxStoredLines) {
        m_lines.pop_front();
        if (m_logScrollOffset > 0) {
            --m_logScrollOffset;
        }
    }
    if (pinnedToBottom) {
        m_logScrollOffset = 0;
    }
}

void ChatWindow::CancelInput() {
    m_input.clear();
    m_inputFocused = false;
    m_charHandlerInstalled = false;
}

void ChatWindow::SyncCharInputHandler(Engine::Platform::Window& window) {
    if (!m_inputFocused) {
        if (m_charHandlerInstalled) {
            window.SetCharInputHandler(nullptr);
            m_charHandlerInstalled = false;
        }
        return;
    }

    if (m_charHandlerInstalled) {
        return;
    }

    window.SetCharInputHandler([this](unsigned int codepoint) {
        if (!m_inputFocused || codepoint < 32 || codepoint == 127) {
            return;
        }
        if (codepoint <= 0x7Fu) {
            AppendTypedChar(static_cast<char>(codepoint));
            return;
        }
        if (codepoint <= 0x7FFu) {
            m_input.push_back(static_cast<char>(0xC0u | ((codepoint >> 6) & 0x1Fu)));
            m_input.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        } else {
            m_input.push_back(static_cast<char>(0xE0u | ((codepoint >> 12) & 0x0Fu)));
            m_input.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
            m_input.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
        if (m_input.size() > Engine::Net::kMaxChatMessageLength) {
            m_input.resize(Engine::Net::kMaxChatMessageLength);
        }
    });
    m_charHandlerInstalled = true;
}

bool ChatWindow::KeyPressed(Engine::Platform::Window& window, int key, bool& wasDown) const {
    GLFWwindow* glfwWindow = window.GetGLFWWindow();
    if (!glfwWindow || key < 0 || key >= 512) {
        return false;
    }
    const bool down = glfwGetKey(glfwWindow, key) == GLFW_PRESS;
    const bool pressed = down && !wasDown;
    wasDown = down;
    return pressed;
}

void ChatWindow::AppendTypedChar(char ch) {
    if (ch == '\0' || m_input.size() >= Engine::Net::kMaxChatMessageLength) {
        return;
    }
    m_input.push_back(ch);
}

void ChatWindow::TrySendInput() {
    if (m_input.empty() || !m_sendHandler) {
        return;
    }
    m_sendHandler(m_input);
    m_input.clear();
}

void ChatWindow::Update(Engine::Platform::Window& window) {
    if (!m_enabled) {
        SyncCharInputHandler(window);
        return;
    }

    HandleScroll(window);

    GLFWwindow* glfwWindow = window.GetGLFWWindow();
    if (!glfwWindow) {
        return;
    }

    const bool ctrlDown = glfwGetKey(glfwWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                          glfwGetKey(glfwWindow, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    const bool lDown = glfwGetKey(glfwWindow, GLFW_KEY_L) == GLFW_PRESS;
    if (IsChatFocusChord(ctrlDown, lDown, m_ctrlLWasDown)) {
        m_inputFocused = true;
        SyncCharInputHandler(window);
        return;
    }
    m_ctrlLWasDown = lDown;

    if (KeyPressed(window, GLFW_KEY_ENTER, m_enterWasDown) ||
        KeyPressed(window, GLFW_KEY_KP_ENTER, m_kpEnterWasDown)) {
        if (!m_inputFocused) {
            if (IsMouseOverPanel(window)) {
                m_inputFocused = true;
            }
        } else {
            TrySendInput();
            m_inputFocused = false;
        }
        SyncCharInputHandler(window);
        return;
    }

    if (!m_inputFocused) {
        SyncCharInputHandler(window);
        return;
    }

    SyncCharInputHandler(window);

    if (KeyPressed(window, GLFW_KEY_ESCAPE, m_escapeWasDown)) {
        CancelInput();
        SyncCharInputHandler(window);
        return;
    }

    if (KeyPressed(window, GLFW_KEY_BACKSPACE, m_backspaceWasDown) && !m_input.empty()) {
        m_input.pop_back();
    }
}

void ChatWindow::Draw(struct nk_context* ctx,
                      const Engine::Platform::UI::FontLibrary& fonts,
                      Engine::Platform::Window& window,
                      int screenWidth,
                      int screenHeight) {
    MaybeReloadLayoutFromManifest();

    if (!m_enabled || !ctx || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    ChatBounds bounds = ResolveBounds(screenWidth, screenHeight);
    bounds.x = m_panelX;
    bounds.y = m_panelY;
    const struct nk_rect panelBounds = nk_rect(bounds.x, bounds.y, bounds.width, bounds.height);
    const struct nk_rect contentBounds = ResolveContentBounds(bounds, m_config.content);

    UpdatePointer(ctx, window, screenWidth, screenHeight, bounds);

    const float idleAlpha = std::clamp(m_config.idleAlpha, 0.0f, 1.0f);
    const float backgroundAlpha =
        (m_hovered || m_inputFocused || m_dragging) ? 1.0f : idleAlpha;

    // Draw the PNG on a fullscreen canvas (same approach as CharacterHud). Layout widgets
    // in a bounded window would otherwise paint over manual canvas commands.
    if (m_texture && m_texture->IsLoaded()) {
        OverlayScope backgroundOverlay(
            ctx,
            "ChatBackground",
            OverlayScope::FullScreen(screenWidth, screenHeight),
            NK_WINDOW_NO_INPUT);
        if (backgroundOverlay.IsOpen() && backgroundOverlay.Canvas()) {
            const nk_byte textureAlpha =
                static_cast<nk_byte>(std::clamp(backgroundAlpha * 255.0f, 0.0f, 255.0f));
            DrawTexture(backgroundOverlay.Canvas(),
                        *m_texture,
                        panelBounds,
                        nk_rgba(255, 255, 255, textureAlpha));
        }
    }

    if (!m_texture || !m_texture->IsLoaded()) {
        OverlayScope overlay(ctx, "ChatWindow", panelBounds, NK_WINDOW_NO_SCROLLBAR);
        if (overlay.IsOpen()) {
            struct nk_command_buffer* canvas = overlay.Canvas();
            if (canvas) {
                const struct nk_rect panel = nk_rect(0.0f, 0.0f, bounds.width, bounds.height);
                const nk_byte panelAlpha =
                    static_cast<nk_byte>(std::clamp(backgroundAlpha * 200.0f, 0.0f, 255.0f));
                nk_fill_rect(canvas, panel, 8.0f, nk_rgba(12, 14, 20, panelAlpha));
                nk_stroke_rect(canvas, panel, 8.0f, 1.0f, nk_rgba(70, 80, 110, panelAlpha));
            }
        }
        return;
    }

    OverlayScope contentOverlay(ctx, "ChatContent", contentBounds, NK_WINDOW_NO_SCROLLBAR);
    if (!contentOverlay.IsOpen()) {
        return;
    }

    const float pad = std::max(0.0f, bounds.padding);
    const nk_color textColor = nk_rgba(230, 235, 245, 255);
    const nk_color hintColor = nk_rgba(170, 178, 195, 255);
    const nk_color scrollbarTrackColor = nk_rgba(40, 48, 64, 120);
    const nk_color scrollbarThumbColor = nk_rgba(150, 165, 195, 200);
    struct nk_font* chatFont = fonts.GetFont(Engine::Platform::UI::FontRole::Chat);
    if (chatFont) {
        nk_style_push_font(ctx, &chatFont->handle);
    }
    nk_style_push_color(ctx, &ctx->style.text.color, textColor);
    // Horizontal inset only — use the full content box height for log + input.
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(pad, 0.0f));
    nk_style_push_vec2(ctx, &ctx->style.text.padding, nk_vec2(0.0f, 0.0f));
    nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0.0f, 0.0f));

    const float contentW = contentBounds.w;
    const float contentH = contentBounds.h;
    const float inputFraction = std::clamp(m_config.content.inputHeightFraction, 0.10f, 0.35f);
    const float lineHeight = std::max(9.0f, bounds.logRowHeight);
    const float inputHeight =
        std::clamp(std::max(bounds.inputRowHeight, lineHeight + 1.0f), lineHeight, contentH * inputFraction);
    const float logHeight = std::max(lineHeight, contentH - inputHeight);

    const size_t lineCount = m_lines.size();
    const int maxVisibleLines =
        std::max(1, static_cast<int>(logHeight / std::max(1.0f, lineHeight)));
    const int maxScroll =
        lineCount > static_cast<size_t>(maxVisibleLines)
            ? static_cast<int>(lineCount) - maxVisibleLines
            : 0;
    m_logScrollOffset = std::clamp(m_logScrollOffset, 0, maxScroll);

    const size_t endLine =
        lineCount > static_cast<size_t>(m_logScrollOffset) ? lineCount - static_cast<size_t>(m_logScrollOffset) : 0;
    const size_t startLine =
        endLine > static_cast<size_t>(maxVisibleLines) ? endLine - static_cast<size_t>(maxVisibleLines) : 0;

    const bool showScrollbar = maxScroll > 0;

    nk_layout_row_dynamic(ctx, logHeight, 1);
    if (nk_group_begin(ctx, "ChatLog", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, lineHeight, 1);
        for (size_t i = startLine; i < endLine; ++i) {
            const ChatLine& line = m_lines[i];
            const std::string formatted =
                line.displayName.empty() ? line.text : (line.displayName + ": " + line.text);
            nk_label(ctx, formatted.c_str(), NK_TEXT_LEFT);
        }
    }
    nk_group_end(ctx);

    if (showScrollbar && contentOverlay.Canvas()) {
        const float trackX = contentW - pad - kScrollbarWidth;
        const float trackY = 0.0f;
        const float trackH = logHeight;
        const float thumbH = std::max(10.0f, trackH * (static_cast<float>(maxVisibleLines) /
                                                       static_cast<float>(std::max(1, static_cast<int>(lineCount)))));
        const float thumbTravel = std::max(0.0f, trackH - thumbH);
        const float thumbY =
            trackY + thumbTravel -
            (maxScroll > 0 ? (static_cast<float>(m_logScrollOffset) / static_cast<float>(maxScroll)) * thumbTravel : 0.0f);
        nk_fill_rect(contentOverlay.Canvas(),
                     nk_rect(trackX, trackY, kScrollbarWidth, trackH),
                     2.0f,
                     scrollbarTrackColor);
        nk_fill_rect(contentOverlay.Canvas(),
                     nk_rect(trackX, thumbY, kScrollbarWidth, thumbH),
                     2.0f,
                     scrollbarThumbColor);
    }

    nk_layout_row_dynamic(ctx, inputHeight, 1);
    if (m_inputFocused) {
        const std::string prompt = "> " + m_input + "_";
        nk_label(ctx, prompt.c_str(), NK_TEXT_LEFT);
    } else if (lineCount == 0) {
        nk_style_push_color(ctx, &ctx->style.text.color, hintColor);
        nk_label(ctx, "Ctrl+L or Enter to type", NK_TEXT_LEFT);
        nk_style_pop_color(ctx);
    } else {
        nk_style_push_color(ctx, &ctx->style.text.color, hintColor);
        nk_label(ctx, "Ctrl+L or Enter to type...", NK_TEXT_LEFT);
        nk_style_pop_color(ctx);
    }

    nk_style_pop_vec2(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_color(ctx);
    if (chatFont) {
        nk_style_pop_font(ctx);
    }

    SyncCharInputHandler(window);
}

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
