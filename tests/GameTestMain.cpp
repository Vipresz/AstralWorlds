#include <gtest/gtest.h>

#include "GameTestSupport.h"
#include "render/Window.h"

#include <memory>

namespace {

class GameRenderEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        Game::Tests::SetWorkingDirectoryToTestRuntime();
        m_window = std::make_unique<Engine::Platform::Window>(64, 64);
        if (m_window->Initialize() != 0) {
            ADD_FAILURE() << "Failed to initialize GLFW/OpenGL for game e2e tests";
            m_window.reset();
        }
    }

    void TearDown() override { m_window.reset(); }

private:
    std::unique_ptr<Engine::Platform::Window> m_window;
};

} // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new GameRenderEnvironment());
    return RUN_ALL_TESTS();
}
