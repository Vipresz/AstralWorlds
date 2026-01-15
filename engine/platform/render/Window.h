#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Engine {
namespace Platform {

class Window {
public:
    Window();
    Window(GLint windowWidth, GLint windowHeight);
    
    int Initialize();
    
    GLint getBufferWidth() { return bufferWidth; }
    GLint getBufferHeight() { return bufferHeight; }
    
    bool getShouldClose() { return glfwWindowShouldClose(mainWindow); }
    void setShouldClose(bool shouldClose) { glfwSetWindowShouldClose(mainWindow, shouldClose ? GL_TRUE : GL_FALSE); }
    
    bool* getsKeys() { return keys; }
    GLfloat getXChange();
    GLfloat getYChange();
    GLfloat getScrollYChange(); // Get mouse wheel scroll delta
    
    void swapBuffers() { glfwSwapBuffers(mainWindow); }
    void pollEvents() { glfwPollEvents(); }
    
    ~Window();

private:
    GLFWwindow* mainWindow;
    
    GLint width, height;
    GLint bufferWidth, bufferHeight;
    
    bool keys[1024];
    
    GLfloat lastX;
    GLfloat lastY;
    GLfloat xChange;
    GLfloat yChange;
    GLfloat scrollYChange;
    bool mouseFirstMoved;
    
    void createCallbacks();
    static void handleKeys(GLFWwindow* window, int key, int code, int action, int mode);
    static void handleMouse(GLFWwindow* window, double xPos, double yPos);
    static void handleScroll(GLFWwindow* window, double xOffset, double yOffset);
};

} // namespace Platform
} // namespace Engine



