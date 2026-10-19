#include <gtest/gtest.h>

#include "ui/ChatInputFocus.h"

TEST(ChatInputFocusTest, CtrlLChordEdgeTriggered) {
    EXPECT_TRUE(Game::UI::IsChatFocusChord(true, true, false));
    EXPECT_FALSE(Game::UI::IsChatFocusChord(true, true, true));
    EXPECT_FALSE(Game::UI::IsChatFocusChord(false, true, false));
    EXPECT_FALSE(Game::UI::IsChatFocusChord(true, false, false));
}
