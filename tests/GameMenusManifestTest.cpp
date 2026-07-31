#include "GameTestSupport.h"

#include <fstream>
#include <gtest/gtest.h>
#include <string>

TEST(GameMenusManifestTest, ManifestContainsChatInputRowHeight) {
    ASSERT_TRUE(Game::Tests::SetWorkingDirectoryToTestRuntime());
    const std::string manifestPath = Game::Tests::RequireAsset("menus/manifest.json");
    std::ifstream manifest(manifestPath);
    ASSERT_TRUE(manifest.good()) << manifestPath << " missing";
    const std::string content((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("\"input_row_height\": 16"), std::string::npos);
    EXPECT_NE(content.find("\"chat\""), std::string::npos);
}

TEST(GameMenusManifestTest, ManifestDefinesHudPanels) {
    ASSERT_TRUE(Game::Tests::SetWorkingDirectoryToTestRuntime());
    const std::string manifestPath = Game::Tests::RequireAsset("menus/manifest.json");
    std::ifstream manifest(manifestPath);
    ASSERT_TRUE(manifest.good()) << manifestPath << " missing";
    const std::string content((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("\"panels\""), std::string::npos);
}
